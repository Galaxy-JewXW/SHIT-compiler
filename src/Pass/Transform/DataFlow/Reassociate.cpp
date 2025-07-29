#include "Pass/Util.h"
#include "Pass/Transforms/Common.h"
#include "Pass/Transforms/DCE.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

// TODO: scalar evolution support

namespace {
std::shared_ptr<Value> base_addr(const std::shared_ptr<Value> &inst) {
    auto ret = inst;
    while (ret->is<BitCast>() != nullptr || ret->is<GetElementPtr>() != nullptr) {
        if (const auto bitcast = ret->is<BitCast>()) {
            ret = bitcast->get_value();
        } else if (const auto gep = ret->is<GetElementPtr>()) {
            ret = gep->get_addr();
        }
    }
    return ret;
}

struct BinaryOpKey {
    IntBinary::Op op_type;
    std::shared_ptr<Value> lhs;
    std::shared_ptr<Value> rhs;

    bool operator==(const BinaryOpKey &other) const {
        return op_type == other.op_type && lhs == other.lhs && rhs == other.rhs;
    }

    BinaryOpKey(const IntBinary::Op &op_type, const std::shared_ptr<Value> &l, const std::shared_ptr<Value> &r) :
        op_type(op_type), lhs(l), rhs(r) {
        const auto a = reinterpret_cast<uintptr_t>(l.get());
        // ReSharper disable once CppTooWideScopeInitStatement
        const auto b = reinterpret_cast<uintptr_t>(r.get());
        if (IntBinary::is_associative_op(op_type) && a > b)
            std::swap(lhs, rhs);
    }
};

struct BinaryOpKeyHasher {
    std::size_t operator()(const BinaryOpKey &k) const {
        const std::size_t h1 = std::hash<int>()(static_cast<int>(k.op_type));
        const std::size_t h2 = std::hash<void *>()(k.lhs.get());
        const std::size_t h3 = std::hash<void *>()(k.rhs.get());
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

struct {
    using Key = std::pair<int, uintptr_t>;

    Key operator()(const std::shared_ptr<Value> &value) const {
        int rank_val;
        if (const auto inst = value->is<Instruction>()) {
            if (inst->get_op() == Operator::LOAD &&
                base_addr(inst->as<Load>()->get_addr())->is<GlobalVariable>() != nullptr) {
                rank_val = 3;
            } else {
                rank_val = 1;
            }
        } else if (value->is<Argument>())
            rank_val = 2;
        else if (value->is_constant())
            rank_val = 4;
        else
            rank_val = 5;
        return {rank_val, reinterpret_cast<uintptr_t>(value.get())};
    }
} ranker;
}

namespace {
class SimpleReassociateImpl {
public:
    explicit SimpleReassociateImpl(const std::shared_ptr<Function> &p_current_function) :
        current_function(p_current_function) {}

    bool run() {
        initialize();
        main_loop();
        cleanup();
        return changed;
    }

private:
    const std::shared_ptr<Function> &current_function;
    std::unordered_set<std::shared_ptr<IntBinary>> worklist{};
    std::unordered_set<std::shared_ptr<Instruction>> to_erase{};
    bool changed{false};
    std::unordered_map<BinaryOpKey, std::shared_ptr<IntBinary>, BinaryOpKeyHasher> value_table;

    static bool is_candidate(const std::shared_ptr<Instruction> &instruction);

    void initialize();

    void main_loop();

    void cleanup() const;

    std::vector<std::shared_ptr<Value>> linearize(const std::shared_ptr<IntBinary> &intbinary);

    std::shared_ptr<Value> rebuild_right_deep_tree(std::vector<std::shared_ptr<Value>> &operand_lists,
                                                   const std::shared_ptr<IntBinary> &origin);

    std::shared_ptr<Value> get_or_create(const std::shared_ptr<Value> &lhs,
                                         const std::shared_ptr<Value> &rhs,
                                         IntBinary::Op type,
                                         const std::shared_ptr<Instruction> &origin);
};

bool SimpleReassociateImpl::is_candidate(const std::shared_ptr<Instruction> &instruction) {
    if (instruction->get_op() == Operator::INTBINARY) {
        return instruction->as<IntBinary>()->is_associative();
    }
    return false;
}

void SimpleReassociateImpl::initialize() {
    for (const auto &block: current_function->get_blocks()) {
        for (const auto &inst: block->get_instructions()) {
            if (!is_candidate(inst))
                continue;
            auto int_binary = inst->as<IntBinary>();

            BinaryOpKey key{int_binary->intbinary_op(), int_binary->get_lhs(), int_binary->get_rhs()};
            value_table[key] = int_binary;

            bool is_root{true};
            for (const auto &user: inst->users()) {
                if (const auto user_inst = user->is<Instruction>()) {
                    if (user_inst->get_op() == Operator::INTBINARY &&
                        user_inst->as<IntBinary>()->intbinary_op() == int_binary->intbinary_op()) {
                        is_root = false;
                        break;
                    }
                } else {
                    log_error("");
                }
            }
            if (is_root) {
                worklist.insert(int_binary);
            }
        }
    }
}

std::shared_ptr<Value> SimpleReassociateImpl::get_or_create(const std::shared_ptr<Value> &lhs,
                                                            const std::shared_ptr<Value> &rhs,
                                                            const IntBinary::Op type,
                                                            const std::shared_ptr<Instruction> &origin) {
    if (lhs->is_constant() && rhs->is_constant()) {
        const auto _l = **lhs->as<ConstInt>(), _r = **rhs->as<ConstInt>();
        const auto _res = [&]() -> int {
            switch (type) {
                case IntBinary::Op::ADD:
                    return _l + _r;
                case IntBinary::Op::MUL:
                    return _l * _r;
                case IntBinary::Op::AND:
                    return _l & _r;
                case IntBinary::Op::OR:
                    return _l | _r;
                case IntBinary::Op::XOR:
                    return _l ^ _r;
                case IntBinary::Op::SMAX:
                    return std::max(_l, _r);
                case IntBinary::Op::SMIN:
                    return std::min(_l, _r);
                default:
                    log_error("Not supported");
            }
        }();
        return ConstInt::create(_res);
    }

    BinaryOpKey key{type, lhs, rhs};
    if (IntBinary::is_associative_op(type)) {
        if (reinterpret_cast<uintptr_t>(lhs.get()) > reinterpret_cast<uintptr_t>(rhs.get())) {
            key.rhs = lhs;
            key.lhs = rhs;
        }
    }

    if (value_table.count(key)) {
        return value_table.at(key);
    }

    static int id = 0;
    auto new_inst = [&]() -> std::shared_ptr<IntBinary> {
        switch (type) {
            case IntBinary::Op::ADD:
                return Add::create("%add" + std::to_string(++id), lhs, rhs, origin->get_block());
            case IntBinary::Op::SUB:
                return Sub::create("%sub" + std::to_string(++id), lhs, rhs, origin->get_block());
            case IntBinary::Op::MUL:
                return Mul::create("%mul" + std::to_string(++id), lhs, rhs, origin->get_block());
            case IntBinary::Op::AND:
                return And::create("%and" + std::to_string(++id), lhs, rhs, origin->get_block());
            case IntBinary::Op::OR:
                return Or::create("%or" + std::to_string(++id), lhs, rhs, origin->get_block());
            case IntBinary::Op::XOR:
                return Xor::create("%xor" + std::to_string(++id), lhs, rhs, origin->get_block());
            case IntBinary::Op::SMAX:
                return Smax::create("%smax" + std::to_string(++id), lhs, rhs, origin->get_block());
            case IntBinary::Op::SMIN:
                return Smin::create("%smin" + std::to_string(++id), lhs, rhs, origin->get_block());
            default:
                log_error("Not supported");
        }
    }();

    Pass::Utils::move_instruction_before(new_inst, origin);
    value_table[key] = new_inst;
    return new_inst;
}

std::vector<std::shared_ptr<Value>> SimpleReassociateImpl::linearize(const std::shared_ptr<IntBinary> &intbinary) {
    std::vector<std::shared_ptr<Value>> operands;
    std::vector<std::pair<std::shared_ptr<Value>, bool>> stack{{intbinary, false}};
    const auto type = intbinary->intbinary_op();

    auto make_negative = [&](const std::shared_ptr<Value> &v) -> std::shared_ptr<Value> {
        if (v->is_constant()) {
            return ConstInt::create(-**v->as<ConstInt>());
        }
        return get_or_create(ConstInt::create(0), v, IntBinary::Op::SUB, intbinary);
    };

    while (!stack.empty()) {
        auto [v, is_negated] = stack.back();
        stack.pop_back();

        if (const auto inst = v->is<IntBinary>()) {
            if (inst->intbinary_op() == type) {
                stack.emplace_back(inst->get_lhs(), is_negated);
                stack.emplace_back(inst->get_rhs(), is_negated);
                continue;
            }
            if (type == IntBinary::Op::ADD && inst->intbinary_op() == IntBinary::Op::SUB) {
                stack.emplace_back(inst->get_lhs(), is_negated);
                stack.emplace_back(inst->get_rhs(), !is_negated);
                continue;
            }
        }

        if (is_negated)
            operands.push_back(make_negative(v));
        else
            operands.push_back(v);
    }
    return operands;
}

std::shared_ptr<Value> SimpleReassociateImpl::rebuild_right_deep_tree(
        std::vector<std::shared_ptr<Value>> &operand_lists,
        const std::shared_ptr<IntBinary> &origin) {
    if (operand_lists.size() <= 2)
        return operand_lists.empty() ? nullptr : operand_lists.front();

    // 为了从右向左构建，先将列表反转
    std::reverse(operand_lists.begin(), operand_lists.end());

    const auto type = origin->intbinary_op();
    auto result = operand_lists.front();

    for (size_t i = 1; i < operand_lists.size(); ++i) {
        // result 始终作为右操作数，形成右斜树
        result = get_or_create(operand_lists[i], result, type, origin);
    }
    return result;
}

void SimpleReassociateImpl::main_loop() {
    while (!worklist.empty()) {
        const auto instruction = *worklist.begin();
        worklist.erase(instruction);

        if (to_erase.count(instruction))
            continue;
        if (instruction->users().size() == 0) {
            to_erase.insert(instruction);
            continue;
        }

        auto operands_list = linearize(instruction);
        if (operands_list.size() <= 2)
            continue;

        std::sort(operands_list.begin(), operands_list.end(),
                  [](const std::shared_ptr<Value> &a, const std::shared_ptr<Value> &b) {
                      return ranker(a) < ranker(b);
                  });

        if (auto new_value = rebuild_right_deep_tree(operands_list, instruction);
            new_value && new_value != instruction) {
            changed = true;
            auto users_copy = instruction->users().lock();

            instruction->replace_by_new_value(new_value);
            instruction->clear_operands();
            to_erase.insert(instruction);
            if (const BinaryOpKey old_key{instruction->intbinary_op(), instruction->get_lhs(), instruction->get_rhs()};
                value_table.count(old_key) && value_table.at(old_key) == instruction) {
                value_table.erase(old_key);
            }

            // 向上/向下/新值 传播
            for (const auto &user: users_copy) {
                if (auto user_inst = user->is<Instruction>()) {
                    if (is_candidate(user_inst))
                        worklist.insert(std::dynamic_pointer_cast<IntBinary>(user_inst));
                } else {
                    log_error("");
                }
            }
            for (const auto &operand: instruction->get_operands()) {
                if (auto op_inst = operand->is<IntBinary>()) {
                    if (is_candidate(op_inst))
                        worklist.insert(op_inst->as<IntBinary>());
                }
            }
            if (auto new_inst = new_value->is<Instruction>()) {
                if (is_candidate(new_inst))
                    worklist.insert(new_inst->as<IntBinary>());
            }
        }
    }
}

void SimpleReassociateImpl::cleanup() const {
    Pass::Utils::delete_instruction_set(Module::instance(), to_erase);
}
}

namespace {
// 参见：https://github.com/llvm/llvm-project/blob/main/llvm/lib/Transforms/Scalar/NaryReassociate.cpp
class [[maybe_unused]] NaryReassociateImpl {
public:
    explicit NaryReassociateImpl(const std::shared_ptr<Function> &current_function,
                                 const Pass::DominanceGraph::Graph &dom_graph) :
        current_function(current_function), dom_graph(dom_graph) {}

    bool run();

private:
    const std::shared_ptr<Function> &current_function;
    const Pass::DominanceGraph::Graph &dom_graph;
    std::unordered_set<std::shared_ptr<IntBinary>> worklist{};
    std::unordered_set<std::shared_ptr<Instruction>> to_erase{};
    bool changed{false};

    static bool is_candidate(const std::shared_ptr<Instruction> &instruction);

    void run_on_block(const std::shared_ptr<Block> &block,
                      const std::unordered_map<BinaryOpKey, std::shared_ptr<IntBinary>, BinaryOpKeyHasher> &value_map);
};

bool NaryReassociateImpl::is_candidate(const std::shared_ptr<Instruction> &instruction) {
    if (instruction->get_op() == Operator::INTBINARY) {
        return instruction->as<IntBinary>()->is_associative();
    }
    return false;
}

void NaryReassociateImpl::run_on_block(const std::shared_ptr<Block> &block,
                                       const std::unordered_map<
                                           BinaryOpKey, std::shared_ptr<IntBinary>, BinaryOpKeyHasher> &value_map) {
    // TODO
    const auto local_map = value_map;
    for (const auto &child: dom_graph.dominance_children.at(block)) {
        run_on_block(child, local_map);
    }
}

bool NaryReassociateImpl::run() {
    std::unordered_map<BinaryOpKey, std::shared_ptr<IntBinary>, BinaryOpKeyHasher> value_table;
    run_on_block(current_function->get_blocks().front(), value_table);
    return changed;
}
}

void Pass::Reassociate::transform(const std::shared_ptr<Module> module) {
    create<AlgebraicSimplify>()->run_on(module);
    create<StandardizeBinary>()->run_on(module);
    const auto dom_info = get_analysis_result<DominanceGraph>(module);
    for (const auto &func: module->get_functions()) {
        if (SimpleReassociateImpl{func}.run()) {
            create<DeadCodeEliminate>()->run_on(func);
        }
        if (NaryReassociateImpl{func, dom_info->graph(func)}.run()) {
            create<DeadCodeEliminate>()->run_on(func);
        }
    }
    create<TreeHeightBalance>()->run_on(module);
    create<DeadCodeEliminate>()->run_on(module);
}
