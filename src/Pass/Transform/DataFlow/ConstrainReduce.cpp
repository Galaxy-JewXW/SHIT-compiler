#include "Pass/Analyses/IntervalAnalysis.h"
#include "Pass/Transforms/Common.h"
#include "Pass/Transforms/DCE.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
[[maybe_unused]]
bool is_back_edge(const std::vector<std::shared_ptr<Pass::Loop>> &loops, const std::shared_ptr<Block> &b,
                  const std::shared_ptr<Block> &pred) {
    for (const auto &loop: loops) {
        if (loop->get_header() != b) {
            continue;
        }
        if (const auto &latchs{loop->get_latch_blocks()};
            std::find_if(latchs.begin(), latchs.end(), [&pred](const auto &latch) { return latch == pred; }) ==
            latchs.end()) {
            continue;
        }
        return true;
    }
    return false;
}

constexpr long max_depth = 100ll;

class Constraint {
public:
    explicit Constraint(const long size = max_depth) : matrix(size, std::vector(size, infinity)), size(size) {
        for (long i = 0; i < size; ++i) {
            matrix[i][i] = 0;
        }
    }

    Constraint(const Constraint &other) = default;

    Constraint &operator=(const Constraint &other) {
        if (this == &other) {
            return *this;
        }
        size = other.size;
        matrix = other.matrix;
        return *this;
    }

    ~Constraint() = default;

    void propagate();

    // icmp提供的数值约束
    void add_relation(long i, long j, Icmp::Op icmp_type);

    // 添加直接的差分约束
    // v_i - v_j <= diff
    void add_relation(long i, long j, long diff);

    [[nodiscard]] std::optional<bool> deduce_relation(long i, long j, Icmp::Op icmp_type);

private:
    std::vector<std::vector<long>> matrix;
    long size;
    static constexpr long infinity = std::numeric_limits<long>::max();
};

void Constraint::propagate() {
    for (long k = 0; k < size; ++k) {
        for (long i = 0; i < size; ++i) {
            for (long j = 0; j < size; ++j) {
                if (matrix[i][k] != infinity && matrix[k][j] != infinity) {
                    matrix[i][j] = std::min(matrix[i][j], matrix[i][k] + matrix[k][j]);
                }
            }
        }
    }
}

void Constraint::add_relation(const long i, const long j, const Icmp::Op icmp_type) {
    switch (icmp_type) {
        case Icmp::Op::EQ: {
            // v_i - v_j <= 0
            // v_j - v_i <= 0
            matrix[i][j] = std::min(matrix[i][j], 0l);
            matrix[j][i] = std::min(matrix[j][i], 0l);
            break;
        }
        // 非对称关系只代表一个单向约束 v_i - v_j <= -1
        // 只需要更新 matrix[i][j] 即可
        case Icmp::Op::LT: {
            matrix[i][j] = std::min(matrix[i][j], -1l);
            break;
        }
        case Icmp::Op::LE: {
            matrix[i][j] = std::min(matrix[i][j], 0l);
            break;
        }
        case Icmp::Op::GT: {
            matrix[j][i] = std::min(matrix[j][i], -1l);
            break;
        }
        case Icmp::Op::GE: {
            matrix[j][i] = std::min(matrix[j][i], 0l);
            break;
        }
        // ReSharper disable once CppDFAUnreachableCode
        default:
            break;
    }
}

// v_i - v_j <= diff
void Constraint::add_relation(const long i, const long j, const long diff) {
    matrix[i][j] = std::min(matrix[i][j], diff);
}

std::optional<bool> Constraint::deduce_relation(const long i, const long j, const Icmp::Op icmp_type) {
    switch (icmp_type) {
        case Icmp::Op::EQ: {
            if (matrix[i][j] <= 0 && matrix[j][i] <= 0)
                return true;
            if (matrix[i][j] <= -1 || matrix[j][i] <= -1)
                return false;
            break;
        }
        case Icmp::Op::NE: {
            if (matrix[i][j] <= 0 && matrix[j][i] <= 0)
                return false;
            if (matrix[i][j] <= -1 || matrix[j][i] <= -1)
                return true;
            break;
        }
        case Icmp::Op::LT: {
            if (matrix[i][j] <= -1)
                return true;
            if (matrix[j][i] <= 0)
                return false;
            break;
        }
        case Icmp::Op::LE: {
            if (matrix[i][j] <= 0)
                return true;
            if (matrix[j][i] <= -1)
                return false;
            break;
        }
        case Icmp::Op::GT:
        case Icmp::Op::GE: {
            return deduce_relation(j, i, Icmp::swap_op(icmp_type));
        }
        default:
            break;
    }
    return std::nullopt;
}

template<typename T>
class Map final : public std::unordered_map<T, int> {
    int cnt_{0};

public:
    int &operator[](const T &key) {
        const auto it = this->find(key);
        if (it == this->end()) {
            this->insert({key, ++cnt_});
        }
        if (this->size() >= max_depth) {
            throw std::runtime_error("");
        }
        return std::unordered_map<T, int>::operator[](key);
    }

    int &cnt() { return cnt_; }
};

class BranchConstrainReduceImpl {
public:
    BranchConstrainReduceImpl(const std::shared_ptr<Function> &current_func,
                              const Pass::ControlFlowGraph::Graph &cfg_graph,
                              const Pass::DominanceGraph::Graph &dom_graph,
                              const std::shared_ptr<Pass::IntervalAnalysis> &interval,
                              const std::vector<std::shared_ptr<Pass::Loop>> &loops) :
        current_func(current_func), cfg_graph(cfg_graph), dom_graph(dom_graph), interval(interval), loops(loops) {}

    bool impl();

private:
    const std::shared_ptr<Function> &current_func;
    const Pass::ControlFlowGraph::Graph &cfg_graph;
    const Pass::DominanceGraph::Graph &dom_graph;
    const std::shared_ptr<Pass::IntervalAnalysis> &interval;
    const std::vector<std::shared_ptr<Pass::Loop>> &loops;

    Map<std::shared_ptr<Value>> id_map{};
    std::unordered_map<std::shared_ptr<Block>, Constraint> constraint_map{};
    bool changed{false};

    template<typename T>
    Pass::IntervalAnalysis::IntervalSet<T> get_interval(const std::shared_ptr<Value> &value,
                                                        const std::shared_ptr<Block> &block);

    void run_on_block(const std::shared_ptr<Block> &block, Constraint &constraint);
};

template<typename T>
[[maybe_unused]]
Pass::IntervalAnalysis::IntervalSet<T> BranchConstrainReduceImpl::get_interval(const std::shared_ptr<Value> &value,
                                                                               const std::shared_ptr<Block> &block) {
    Pass::IntervalAnalysis::IntervalSet<T> res;
    if (value->is_constant()) {
        const auto constant = value->as<Const>()->get_constant_value();
        res = std::visit([](const auto c) { return Pass::IntervalAnalysis::IntervalSet<T>(c); }, constant);
    } else if (const auto inst = value->is<Instruction>()) {
        const auto ctx = interval->ctx_after(inst, block);
        res = std::get<Pass::IntervalAnalysis::IntervalSet<T>>(ctx.get(inst));
    } else if (const auto arg = value->is<Argument>()) {
        const auto ctx = interval->ctx_after(block->get_instructions().back(), block);
        res = std::get<Pass::IntervalAnalysis::IntervalSet<T>>(ctx.get(arg));
    } else
        log_error("Unsupported value: %s", value->to_string().c_str());
    if (res.is_undefined()) {
        return Pass::IntervalAnalysis::IntervalSet<T>::make_any();
    }
    return res;
}

void BranchConstrainReduceImpl::run_on_block(const std::shared_ptr<Block> &block, Constraint &constraint) {
    constraint_map[block] = constraint;

    for (const auto &inst: block->get_instructions()) {
        if (inst->get_op() != Operator::INTBINARY)
            continue;
        const auto intbinary = inst->as<IntBinary>();
        const auto lhs = intbinary->get_lhs(), rhs = intbinary->get_rhs();
        const int intbinary_id = id_map[intbinary];
        switch (intbinary->intbinary_op()) {
            case IntBinary::Op::ADD: {
                // c = a + b
                if (rhs->is_constant()) {
                    // b是常数 => c - a = b, a - c = -b
                    // c - a <= b, a - c <= -b
                    const auto constant_rhs = **rhs->as<ConstInt>();
                    const int idx = id_map[lhs];
                    constraint.add_relation(intbinary_id, idx, constant_rhs);
                    constraint.add_relation(idx, intbinary_id, -constant_rhs);
                    break;
                }
                break;
            }
            case IntBinary::Op::SUB: {
                // c = a - b;
                if (rhs->is_constant()) {
                    // b是常数 => c - a = -b, a - c = b
                    // c - a <= -b, a - c <= b
                    const auto constant_rhs = **rhs->as<ConstInt>();
                    const int idx = id_map[lhs];
                    constraint.add_relation(intbinary_id, idx, -constant_rhs);
                    constraint.add_relation(idx, intbinary_id, constant_rhs);
                    break;
                }
                break;
            }
            case IntBinary::Op::MUL: {
                // c = a * b;
                if (!lhs->is_constant()) {
                    const int idx = id_map[lhs];
                    if (rhs->is_constant() && **rhs->as<ConstInt>() == 1) {
                        // c = a * 1 => c = a;
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::EQ);
                    }
                }
                if (!rhs->is_constant()) {
                    const int idx = id_map[rhs];
                    if (lhs->is_constant() && **lhs->as<ConstInt>() == 1) {
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::EQ);
                    }
                }
                break;
            }
            case IntBinary::Op::DIV: {
                // c = a / b
                if (!lhs->is_constant()) {
                    const int idx = id_map[lhs];
                    if (rhs->is_constant() && **rhs->as<ConstInt>() == 1) {
                        // c = a / 1 => c = a;
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::EQ);
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    constraint.propagate();
    if (const auto terminator{block->get_instructions().back()}; terminator->get_op() == Operator::BRANCH) {
        const auto branch = terminator->as<Branch>();
        if (const auto icmp = branch->get_cond()->is<Icmp>()) {
            if (const auto lhs = icmp->get_lhs(), rhs = icmp->get_rhs(); !lhs->is_constant() && !rhs->is_constant()) {
                const auto idx1 = id_map[lhs], idx2 = id_map[rhs];
                if (const auto res = constraint.deduce_relation(idx1, idx2, icmp->icmp_op())) {
                    block->get_instructions().pop_back();
                    changed = true;
                    if (res.value()) {
                        Jump::create(branch->get_true_block(), block);
                    } else {
                        Jump::create(branch->get_false_block(), block);
                    }
                }
            }
        }
    }

    for (const auto &child: dom_graph.dominance_children.at(block)) {
        auto child_constraint = constraint;
        if (const auto terminator = block->get_instructions().back(); terminator->get_op() == Operator::BRANCH) {
            const auto branch = terminator->as<Branch>();
            if (const auto icmp = terminator->as<Branch>()->get_cond()->is<Icmp>()) {
                if (const auto &lhs = icmp->get_lhs(), &rhs = icmp->get_rhs();
                    !lhs->is_constant() && !rhs->is_constant()) {
                    const auto idx1 = id_map[lhs], idx2 = id_map[rhs];
                    const auto icmp_op = icmp->icmp_op();

                    if (cfg_graph.predecessors.at(child).size() != 1)
                        continue;

                    if (child == branch->get_true_block()) {
                        child_constraint.add_relation(idx1, idx2, icmp_op);
                    } else if (child == branch->get_false_block()) {
                        child_constraint.add_relation(idx1, idx2, Icmp::inverse_op(icmp_op));
                    }
                }
            }
        }
        run_on_block(child, child_constraint);
    }
}

bool BranchConstrainReduceImpl::impl() {
    Constraint constraint{};
    try {
        run_on_block(current_func->get_blocks().front(), constraint);
    } catch (const std::exception &) {
    }
    return changed;
}
} // namespace

namespace Pass {
void ConstrainReduce::transform(const std::shared_ptr<Module> module) {
    create<StandardizeBinary>()->run_on(module);
    const auto cfg_info = get_analysis_result<ControlFlowGraph>(module);
    const auto dom_info = get_analysis_result<DominanceGraph>(module);
    const auto loop_info = get_analysis_result<LoopAnalysis>(module);
    for (const auto &func: module->get_functions()) {
        // ReSharper disable once CppTooWideScopeInitStatement
        BranchConstrainReduceImpl impl{func, cfg_info->graph(func), dom_info->graph(func), nullptr,
                                       loop_info->loops(func)};
        if (impl.impl()) {
            set_analysis_result_dirty<ControlFlowGraph>(func);
            set_analysis_result_dirty<DominanceGraph>(func);
        }
    }
    create<DeadInstEliminate>()->run_on(module);
}
} // namespace Pass
