#include <type_traits>

#include "Mir/Builder.h"
#include "Pass/Transforms/Common.h"
#include "Pass/Transforms/DCE.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;
using FunctionPtr = std::shared_ptr<Function>;
using BlockPtr = std::shared_ptr<Block>;
using InstructionPtr = std::shared_ptr<Instruction>;

namespace {
std::string get_hash(const std::shared_ptr<GetElementPtr> &instruction) {
    std::ostringstream oss;
    oss << "gep";
    for (const auto &operand: instruction->get_operands()) {
        oss << " " << operand->get_name();
    }
    return oss.str();
}

std::string get_hash(const std::shared_ptr<Fcmp> &instruction) {
    return "fcmp " + std::to_string(static_cast<int>(instruction->op)) + " " + instruction->get_lhs()->get_name() +
           " " + instruction->get_rhs()->get_name();
}

std::string get_hash(const std::shared_ptr<Icmp> &instruction) {
    return "icmp " + std::to_string(static_cast<int>(instruction->op)) + " " + instruction->get_lhs()->get_name() +
           " " + instruction->get_rhs()->get_name();
}

std::string get_hash(const std::shared_ptr<IntBinary> &instruction) {
    auto lhs_hash = instruction->get_lhs()->get_name(), rhs_hash = instruction->get_rhs()->get_name();
    if (instruction->is_commutative() && lhs_hash >= rhs_hash) {
        std::swap(lhs_hash, rhs_hash);
    }
    return "intbinary " + std::to_string(static_cast<int>(instruction->op)) + " " + lhs_hash + " " + rhs_hash;
}

std::string get_hash(const std::shared_ptr<FloatBinary> &instruction) {
    auto lhs_hash = instruction->get_lhs()->get_name(), rhs_hash = instruction->get_rhs()->get_name();
    if (instruction->is_commutative() && lhs_hash >= rhs_hash) {
        std::swap(lhs_hash, rhs_hash);
    }
    return "floatbinary " + std::to_string(static_cast<int>(instruction->op)) + " " + lhs_hash + " " + rhs_hash;
}

std::string get_hash(const std::shared_ptr<FloatTernary> &instruction) {
    const auto &x_hash = instruction->get_x()->get_name(), &y_hash = instruction->get_y()->get_name(),
               &z_hash = instruction->get_z()->get_name();
    return "floatternary " + std::to_string(static_cast<int>(instruction->floatternary_op())) + " " + x_hash + " " +
           y_hash + " " + z_hash;
}

std::string get_hash(const std::shared_ptr<FNeg> &instruction) {
    const auto &value_hash = instruction->get_value()->get_name();
    return "fneg " + value_hash;
}

std::string get_hash(const std::shared_ptr<Zext> &instruction) {
    return "zext " + instruction->get_value()->get_name() + " " + instruction->get_value()->get_type()->to_string() +
           " " + instruction->get_type()->to_string();
}

std::string get_hash(const std::shared_ptr<Fptosi> &instruction) {
    return "fptosi " + instruction->get_value()->get_name() + " " + instruction->get_value()->get_type()->to_string() +
           " " + instruction->get_type()->to_string();
}

std::string get_hash(const std::shared_ptr<Sitofp> &instruction) {
    return "sitofp " + instruction->get_value()->get_name() + " " + instruction->get_value()->get_type()->to_string() +
           " " + instruction->get_type()->to_string();
}

std::string get_hash(const std::shared_ptr<Call> &instruction,
                     const std::shared_ptr<Pass::FunctionAnalysis> &func_analysis) {
    const auto &func = instruction->get_function()->as<Function>();
    if (func->is_runtime_func()) {
        return "";
    }
    if (const auto &func_info = func_analysis->func_info(func);
        func_info.has_return && func_info.no_state && !func_info.io_read && !func_info.io_write) {
        std::ostringstream oss;
        for (const auto &operand: instruction->get_params()) {
            oss << " " << operand->get_name() << ",";
        }
        return "call " + func->get_name() + " " + oss.str();
    }
    return "";
}

std::string get_instruction_hash(const InstructionPtr &instruction,
                                 const std::shared_ptr<Pass::FunctionAnalysis> &func_analysis) {
    switch (instruction->get_op()) {
        case Operator::GEP:
            return get_hash(instruction->as<GetElementPtr>());
        case Operator::FCMP:
            return get_hash(instruction->as<Fcmp>());
        case Operator::ICMP:
            return get_hash(instruction->as<Icmp>());
        case Operator::INTBINARY:
            return get_hash(instruction->as<IntBinary>());
        case Operator::FLOATBINARY:
            return get_hash(instruction->as<FloatBinary>());
        case Operator::FLOATTERNARY:
            return get_hash(instruction->as<FloatTernary>());
        case Operator::FNEG:
            return get_hash(instruction->as<FNeg>());
        case Operator::ZEXT:
            return get_hash(instruction->as<Zext>());
        case Operator::SITOFP:
            return get_hash(instruction->as<Sitofp>());
        case Operator::FPTOSI:
            return get_hash(instruction->as<Fptosi>());
        case Operator::CALL:
            return get_hash(instruction->as<Call>(), func_analysis);
        default:
            return "";
    }
}

template<typename>
struct always_false : std::false_type {};

// 声明模板
template<typename Binary>
struct binary_traits {
    static_assert(always_false<Binary>::value, "Unsupported Binary type for evaluate_binary");
};

template<>
struct binary_traits<IntBinary> {
    using value_type = int;
    using constant_type = ConstInt;
};

template<>
struct binary_traits<FloatBinary> {
    using value_type = double;
    using constant_type = ConstFloat;
};

template<typename Binary>
bool evaluate_binary(const std::shared_ptr<Binary> &inst, typename binary_traits<Binary>::value_type &res) {
    // 自动推导操作数的值类型和常量类型
    using T = typename binary_traits<Binary>::value_type;
    using ConstType = typename binary_traits<Binary>::constant_type;
    // 如果任一操作数不是常量，则无法进行常量折叠，返回false
    const auto &lhs = inst->get_lhs(), &rhs = inst->get_rhs();
    if (!lhs->is_constant() || !rhs->is_constant())
        return false;
    if constexpr (std::is_same_v<T, int>) {
        if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            log_error("Illegal operator type for %s", inst->to_string().c_str());
        }
    } else if constexpr (std::is_same_v<T, double>) {
        if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
            log_error("Illegal operator type for %s", inst->to_string().c_str());
        }
    }
    const auto lhs_val = lhs->template as<ConstType>()->get_constant_value(),
               rhs_val = rhs->template as<ConstType>()->get_constant_value();
    try {
        if constexpr (std::is_same_v<Binary, IntBinary>) {
            static_assert(std::is_same_v<T, int>);
            res = [&]() -> T {
                switch (inst->op) {
                    case IntBinary::Op::AND:
                        return lhs_val.template get<T>() & rhs_val.template get<T>();
                    case IntBinary::Op::OR:
                        return lhs_val.template get<T>() | rhs_val.template get<T>();
                    case IntBinary::Op::XOR:
                        return lhs_val.template get<T>() ^ rhs_val.template get<T>();
                    default:
                        return 0;
                }
            }();
        }
        const auto _res = [&]() -> eval_t {
            switch (inst->op) {
                case Binary::Op::ADD:
                    return lhs_val + rhs_val;
                case Binary::Op::SUB:
                    return lhs_val - rhs_val;
                case Binary::Op::MUL:
                    return lhs_val * rhs_val;
                case Binary::Op::DIV:
                    return lhs_val / rhs_val;
                case Binary::Op::MOD:
                    return lhs_val % rhs_val;
                case Binary::Op::SMAX:
                    return std::max(lhs_val, rhs_val);
                case Binary::Op::SMIN:
                    return std::min(lhs_val, rhs_val);
                default:
                    throw std::runtime_error("");
            }
        }();
        res = _res.template get<T>();
    } catch (const std::exception &) {
        return false;
    }
    return true;
}

template<typename Cmp>
struct cmp_traits {
    static_assert(always_false<Cmp>::value, "Unsupported Cmp type for evaluate_cmp");
};

// 对Icmp进行特化
template<>
struct cmp_traits<Icmp> {
    using value_type = int;
    using constant_type = ConstInt;
};

// 对Fcmp进行特化
template<>
struct cmp_traits<Fcmp> {
    using value_type = double;
    using constant_type = ConstFloat;
};

// 修改后的evaluate_cmp函数，只需传入指令类型Cmp，其余类型自动推导
template<typename Cmp>
bool evaluate_cmp(const std::shared_ptr<Cmp> &inst, int &res) {
    // 自动推导比较操作的数值类型和对应的常量类型
    using T = typename cmp_traits<Cmp>::value_type;
    using ConstType = typename cmp_traits<Cmp>::constant_type;
    // 如果任一操作数不是常量，则无法进行折叠，返回false
    const auto &lhs = inst->get_lhs(), &rhs = inst->get_rhs();
    if (!lhs->is_constant() || !rhs->is_constant())
        return false;
    // 检查左右操作数的类型是否合法
    if constexpr (std::is_same_v<T, int>) {
        if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            log_error("Illegal operator type for %s", inst->to_string().c_str());
        }
    } else if constexpr (std::is_same_v<T, double>) {
        if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
            log_error("Illegal operator type for %s", inst->to_string().c_str());
        }
    }
    const auto lhs_val = **lhs->template as<ConstType>(), rhs_val = **rhs->template as<ConstType>();
    try {
        res = [&]() -> int {
            switch (inst->op) {
                case Cmp::Op::EQ:
                    return lhs_val == rhs_val;
                case Cmp::Op::NE:
                    return lhs_val != rhs_val;
                case Cmp::Op::GT:
                    return lhs_val > rhs_val;
                case Cmp::Op::GE:
                    return lhs_val >= rhs_val;
                case Cmp::Op::LT:
                    return lhs_val < rhs_val;
                case Cmp::Op::LE:
                    return lhs_val <= rhs_val;
                default:
                    throw std::runtime_error("");
            }
        }();
    } catch (const std::exception &) {
        return false;
    }
    return true;
}

bool evaluate_float_ternary(const std::shared_ptr<FloatTernary> &inst, double &res) {
    const auto &x = inst->get_x(), &y = inst->get_y(), &z = inst->get_z();
    if (!x->is_constant() || !y->is_constant() || !z->is_constant())
        return false;
    if (!x->get_type()->is_float() || !y->get_type()->is_float() || !z->get_type()->is_float()) {
        log_error("Illegal operator type for %s", inst->to_string().c_str());
    }
    const auto x_val = **x->as<ConstFloat>(), y_val = **y->as<ConstFloat>(), z_val = **z->as<ConstFloat>();
    res = [&]() -> double {
        switch (inst->floatternary_op()) {
            case FloatTernary::Op::FMADD:
                return x_val * y_val + z_val;
            case FloatTernary::Op::FNMADD:
                return -(x_val * y_val + z_val);
            case FloatTernary::Op::FMSUB:
                return x_val * y_val - z_val;
            case FloatTernary::Op::FNMSUB:
                return -(x_val * y_val - z_val);
        }
        log_error("Invalid float ternary op");
    }();
    return true;
}

bool evaluate_fneg(const std::shared_ptr<FNeg> &inst, double &res) {
    const auto &value{inst->get_value()};
    if (!value->is_constant())
        return false;
    if (!value->get_type()->is_float()) {
        log_error("Illegal operator type for %s", inst->to_string().c_str());
    }
    res = -value->as<ConstFloat>()->get_constant_value().get<double>();
    return true;
}
} // namespace

namespace Pass {
bool LocalValueNumbering::fold_instruction(const std::shared_ptr<Instruction> &instruction) {
    switch (instruction->get_op()) {
        case Operator::INTBINARY: {
            const auto int_binary = instruction->as<IntBinary>();
            if (int res_val; evaluate_binary(int_binary, res_val)) {
                const auto const_int = ConstInt::create(res_val);
                int_binary->replace_by_new_value(const_int);
                return true;
            }
            break;
        }
        case Operator::FLOATBINARY: {
            const auto float_binary = instruction->as<FloatBinary>();
            if (double res_val; evaluate_binary(float_binary, res_val)) {
                const auto const_float = ConstFloat::create(res_val);
                float_binary->replace_by_new_value(const_float);
                return true;
            }
            break;
        }
        case Operator::ICMP: {
            const auto icmp = instruction->as<Icmp>();
            if (int res_val; evaluate_cmp(icmp, res_val)) {
                const auto const_bool = ConstBool::create(res_val);
                icmp->replace_by_new_value(const_bool);
                return true;
            }
            break;
        }
        case Operator::FCMP: {
            const auto fcmp = instruction->as<Fcmp>();
            if (int res_val; evaluate_cmp(fcmp, res_val)) {
                const auto const_bool = ConstBool::create(res_val);
                fcmp->replace_by_new_value(const_bool);
                return true;
            }
            break;
        }
        case Operator::ZEXT: {
            const auto zext = instruction->as<Zext>();
            if (const auto const_val = zext->get_value(); const_val->is_constant()) {
                const auto const_int = type_cast(const_val, zext->get_type(), nullptr);
                zext->replace_by_new_value(const_int);
                return true;
            }
            break;
        }
        case Operator::SITOFP: {
            const auto sitofp = instruction->as<Sitofp>();
            if (const auto const_val = sitofp->get_value(); const_val->is_constant()) {
                const auto const_float = type_cast(const_val, sitofp->get_type(), nullptr);
                sitofp->replace_by_new_value(const_float);
                return true;
            }
            break;
        }
        case Operator::FPTOSI: {
            const auto fptosi = instruction->as<Fptosi>();
            if (const auto const_val = fptosi->get_value(); const_val->is_constant()) {
                const auto const_int = type_cast(const_val, fptosi->get_type(), nullptr);
                fptosi->replace_by_new_value(const_int);
                return true;
            }
            break;
        }
        case Operator::FLOATTERNARY: {
            const auto float_ternary = instruction->as<FloatTernary>();
            if (double res_val; evaluate_float_ternary(float_ternary, res_val)) {
                const auto const_float = ConstFloat::create(res_val);
                float_ternary->replace_by_new_value(const_float);
                return true;
            }
            break;
        }
        case Operator::FNEG: {
            const auto fneg = instruction->as<FNeg>();
            if (double res_val; evaluate_fneg(fneg, res_val)) {
                const auto const_float = ConstFloat::create(res_val);
                fneg->replace_by_new_value(const_float);
                return true;
            }
        }
        default:
            break;
    }
    return false;
}

bool LocalValueNumbering::run_on_block(const FunctionPtr &func, const BlockPtr &block,
                                        std::unordered_map<std::string, InstructionPtr> &value_hashmap) {
    bool changed = false;
    std::vector<std::string> local_hashes;

    for (auto it = block->get_instructions().begin(); it != block->get_instructions().end();) {
        InstructionPtr current_inst = *it;
        if (fold_instruction(current_inst)) {
            current_inst->clear_operands();
            it = block->get_instructions().erase(it);
            changed = true;
            continue;
        }

        const auto &instruction_hash = get_instruction_hash(current_inst, func_analysis);
        if (instruction_hash.empty()) {
            ++it;
            continue;
        }

        if (value_hashmap.count(instruction_hash)) {
            InstructionPtr candidate_inst = value_hashmap.at(instruction_hash);

            // 通过状态回溯，可以保证哈希表中的候选项总是支配当前块
            current_inst->replace_by_new_value(candidate_inst);
            current_inst->clear_operands();
            it = block->get_instructions().erase(it);
            changed = true;
            continue;
        }

        value_hashmap[instruction_hash] = current_inst;
        local_hashes.push_back(instruction_hash);
        ++it;
    }
    for (const auto &child: dom_info->graph(func).dominance_children.at(block)) {
        changed |= run_on_block(func, child, value_hashmap);
    }
    for (const auto &hash : local_hashes) {
        value_hashmap.erase(hash);
    }

    return changed;
}

bool LocalValueNumbering::run_on_func(const FunctionPtr &func) {
    const auto &entry_block = func->get_blocks().front();
    std::unordered_map<std::string, InstructionPtr> value_hashmap;
    return run_on_block(func, entry_block, value_hashmap);
}

void LocalValueNumbering::transform(const std::shared_ptr<Module> module) {
    dom_info = get_analysis_result<DominanceGraph>(module);
    func_analysis = get_analysis_result<FunctionAnalysis>(module);
    create<AlgebraicSimplify>()->run_on(module);
    // 不同的遍历顺序可能导致化简的结果不同
    bool changed = false;
    do {
        changed = false;
        for (const auto &func: *module) {
            changed |= run_on_func(func);
        }
    } while (changed);
    do {
        changed = false;
        for (const auto &func: *module) {
            changed |= run_on_func(func);
        }
    } while (changed);
    dom_info = nullptr;
    func_analysis = nullptr;
    create<AlgebraicSimplify>()->run_on(module);
    create<DeadInstEliminate>()->run_on(module);
}
} // namespace Pass
