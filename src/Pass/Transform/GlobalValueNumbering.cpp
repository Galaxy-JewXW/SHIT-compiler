#include "Pass/Transform.h"

#include "Mir/Builder.h"

using namespace Mir;
using FunctionPtr = std::shared_ptr<Function>;
using BlockPtr = std::shared_ptr<Block>;
using InstructionPtr = std::shared_ptr<Instruction>;

namespace {
// TODO：为什么Fptosi, Sitofp不能做GVN优化？
std::string get_hash(const std::shared_ptr<GetElementPtr> &instruction) {
    std::ostringstream oss;
    oss << "gep";
    for (const auto &operand: instruction->get_operands()) {
        oss << " " << operand->get_name();
    }
    return oss.str();
}

std::string get_hash(const std::shared_ptr<Fcmp> &instruction) {
    return "fcmp " + std::to_string(static_cast<int>(instruction->op)) + " " +
           instruction->get_lhs()->get_name() + " " + instruction->get_rhs()->get_name();
}

std::string get_hash(const std::shared_ptr<Icmp> &instruction) {
    return "icmp " + std::to_string(static_cast<int>(instruction->op)) + " " +
           instruction->get_lhs()->get_name() + " " + instruction->get_rhs()->get_name();
}

std::string get_hash(const std::shared_ptr<IntBinary> &instruction) {
    switch (instruction->op) {
        case IntBinary::Op::ADD:
        case IntBinary::Op::MUL: {
            auto lhs_hash = instruction->get_lhs()->get_name(),
                 rhs_hash = instruction->get_rhs()->get_name();
            if (lhs_hash >= rhs_hash) {
                std::swap(lhs_hash, rhs_hash);
            }
            return "intbinary " + std::to_string(static_cast<int>(instruction->op)) + " " +
                   lhs_hash + " " + rhs_hash;
        }
        default: {
            const auto &lhs_hash = instruction->get_lhs()->get_name(),
                       &rhs_hash = instruction->get_rhs()->get_name();
            return "intbinary " + std::to_string(static_cast<int>(instruction->op)) + " " +
                   lhs_hash + " " + rhs_hash;
        }
    }
}

std::string get_hash(const std::shared_ptr<FloatBinary> &instruction) {
    switch (instruction->op) {
        case FloatBinary::Op::ADD:
        case FloatBinary::Op::MUL: {
            auto lhs_hash = instruction->get_lhs()->get_name(),
                 rhs_hash = instruction->get_rhs()->get_name();
            if (lhs_hash >= rhs_hash) {
                std::swap(lhs_hash, rhs_hash);
            }
            return "floatbinary " + std::to_string(static_cast<int>(instruction->op)) + " " +
                   lhs_hash + " " + rhs_hash;
        }
        default: {
            const auto &lhs_hash = instruction->get_lhs()->get_name(),
                       &rhs_hash = instruction->get_rhs()->get_name();
            return "floatbinary " + std::to_string(static_cast<int>(instruction->op)) + " " +
                   lhs_hash + " " + rhs_hash;
        }
    }
}

std::string get_hash(const std::shared_ptr<Zext> &instruction) {
    return "zext " + instruction->get_value()->get_name() + " "
           + instruction->get_value()->get_type()->to_string() + " " + instruction->get_type()->to_string();
}

std::string get_instruction_hash(const InstructionPtr &instruction) {
    switch (instruction->get_op()) {
        case Operator::GEP:
            return get_hash(std::static_pointer_cast<GetElementPtr>(instruction));
        case Operator::FCMP:
            return get_hash(std::static_pointer_cast<Fcmp>(instruction));
        case Operator::ICMP:
            return get_hash(std::static_pointer_cast<Icmp>(instruction));
        case Operator::INTBINARY:
            return get_hash(std::static_pointer_cast<IntBinary>(instruction));
        case Operator::FLOATBINARY:
            return get_hash(std::static_pointer_cast<FloatBinary>(instruction));
        case Operator::ZEXT:
            return get_hash(std::static_pointer_cast<Zext>(instruction));
        default:
            return "";
    }
}

template<typename T>
struct always_false : std::false_type {};

// 声明模板
template<typename Binary>
struct binary_traits;

// 对IntBinary指令进行特化
template<>
struct binary_traits<IntBinary> {
    using value_type = int;
    using constant_type = ConstInt;
};

// 对FloatBinary指令进行特化
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
    const auto lhs = inst->get_lhs(), rhs = inst->get_rhs();
    if (!lhs->is_constant() || !rhs->is_constant()) return false;
    if constexpr (std::is_same_v<T, int>) {
        if (!lhs->get_type()->is_int32() || !rhs->get_type()->is_int32()) {
            log_error("Illegal operator type for %s", inst->to_string().c_str());
        }
    } else if constexpr (std::is_same_v<T, double>) {
        if (!lhs->get_type()->is_float() || !rhs->get_type()->is_float()) {
            log_error("Illegal operator type for %s", inst->to_string().c_str());
        }
    }
    // 从常量指令中提取数值，根据操作符进行计算
    const auto lhs_val = **lhs->template as<ConstType>(),
               rhs_val = **rhs->template as<ConstType>();
    switch (inst->op) {
        case Binary::Op::ADD: res = lhs_val + rhs_val;
            break;
        case Binary::Op::SUB: res = lhs_val - rhs_val;
            break;
        case Binary::Op::MUL: res = lhs_val * rhs_val;
            break;
        case Binary::Op::DIV: res = lhs_val / rhs_val;
            break;
        case Binary::Op::MOD: {
            if constexpr (std::is_same_v<T, double>) {
                res = std::fmod(lhs_val, rhs_val);
            } else {
                res = lhs_val % rhs_val;
            }
            break;
        }
        default: log_error("Unsupported operator in %s", inst->to_string().c_str());
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
    const auto lhs = inst->get_lhs(), rhs = inst->get_rhs();
    if (!lhs->is_constant() || !rhs->is_constant()) return false;
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
    // 解引用静态指针获得常量值，根据比较操作符进行计算
    const auto lhs_val = **lhs->template as<ConstType>(),
               rhs_val = **rhs->template as<ConstType>();
    switch (inst->op) {
        case Cmp::Op::EQ: res = lhs_val == rhs_val;
            break;
        case Cmp::Op::NE: res = lhs_val != rhs_val;
            break;
        case Cmp::Op::GT: res = lhs_val > rhs_val;
            break;
        case Cmp::Op::GE: res = lhs_val >= rhs_val;
            break;
        case Cmp::Op::LT: res = lhs_val < rhs_val;
            break;
        case Cmp::Op::LE: res = lhs_val <= rhs_val;
            break;
        default: log_error("Unsupported operator in %s", inst->to_string().c_str());
    }
    return true;
}
}

namespace Pass {
bool GlobalValueNumbering::fold_instruction(const std::shared_ptr<Instruction> &instruction) {
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
        default: break;
    }
    return false;
}

bool GlobalValueNumbering::run_on_block(const FunctionPtr &func,
                                        const BlockPtr &block,
                                        std::unordered_map<std::string, InstructionPtr> &value_hashmap) {
    bool changed = false;
    for (auto it = block->get_instructions().begin(); it != block->get_instructions().end();) {
        if (fold_instruction(*it)) {
            (*it)->clear_operands();
            it = block->get_instructions().erase(it);
            changed = true;
        }
        const auto &instruction_hash = get_instruction_hash(*it);
        if (instruction_hash.empty()) {
            ++it;
            continue;
        }
        if (value_hashmap.find(instruction_hash) != value_hashmap.end()) {
            (*it)->replace_by_new_value(value_hashmap[instruction_hash]);
            (*it)->clear_operands();
            it = block->get_instructions().erase(it);
            changed = true;
        } else {
            value_hashmap[instruction_hash] = *it;
            ++it;
        }
    }
    for (const auto &child: cfg->dominance_children(func).at(block)) {
        changed |= run_on_block(func, child, value_hashmap);
    }
    return changed;
}

bool GlobalValueNumbering::run_on_func(const FunctionPtr &func) {
    const auto &entry_block = func->get_blocks().front();
    std::unordered_map<std::string, InstructionPtr> value_hashmap;
    return run_on_block(func, entry_block, value_hashmap);
}

void GlobalValueNumbering::transform(const std::shared_ptr<Module> module) {
    cfg = get_analysis_result<ControlFlowGraph>(module);
    create<AlgebraicSimplify>()->run_on(module);
    // 不同的遍历顺序可能导致化简的结果不同
    // 跑多次GVN直到一个不动点
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
    cfg = nullptr;
    // GVN后可能出现一条指令被替换成其另一条指令，但是那条指令并不支配这条指令的users的问题
    // 可以通过 GCM 解决。在 GCM 中考虑value之间的依赖，会根据依赖将那条指令移动到正确的位置
    create<GlobalCodeMotion>()->run_on(module);
    create<AlgebraicSimplify>()->run_on(module);
}
}
