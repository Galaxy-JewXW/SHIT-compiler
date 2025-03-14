#include "Pass/Transform.h"

using namespace Mir;
using FunctionPtr = std::shared_ptr<Function>;
using BlockPtr = std::shared_ptr<Block>;
using InstructionPtr = std::shared_ptr<Instruction>;

static std::shared_ptr<Pass::ControlFlowGraph> cfg = nullptr;

// TODO：为什么Fptosi, Sitofp不能做GVN优化？
static std::string get_hash(const std::shared_ptr<GetElementPtr> &instruction) {
    std::ostringstream oss;
    oss << "gep";
    for (const auto &operand: instruction->get_operands()) {
        oss << " " << operand->get_name();
    }
    return oss.str();
}

static std::string get_hash(const std::shared_ptr<Fcmp> &instruction) {
    return "fcmp " + std::to_string(static_cast<int>(instruction->op)) + " " +
           instruction->get_lhs()->get_name() + " " + instruction->get_rhs()->get_name();
}

static std::string get_hash(const std::shared_ptr<Icmp> &instruction) {
    return "icmp " + std::to_string(static_cast<int>(instruction->op)) + " " +
           instruction->get_lhs()->get_name() + " " + instruction->get_rhs()->get_name();
}

static std::string get_hash(const std::shared_ptr<IntBinary> &instruction) {
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

static std::string get_hash(const std::shared_ptr<FloatBinary> &instruction) {
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

static std::string get_hash(const std::shared_ptr<Zext> &instruction) {
    return "zext " + instruction->get_value()->get_name() + " "
           + instruction->get_value()->get_type()->to_string() + " " + instruction->get_type()->to_string();
}

static std::string get_instruction_hash(const InstructionPtr &instruction) {
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

static void run_on_block(const FunctionPtr &func,
                         const BlockPtr &block,
                         std::unordered_map<std::string, InstructionPtr> &value_hashmap) {
    for (auto it = block->get_instructions().begin(); it != block->get_instructions().end();) {
        const auto &instruction_hash = get_instruction_hash(*it);
        if (instruction_hash.empty()) {
            ++it;
            continue;
        }
        if (value_hashmap.find(instruction_hash) != value_hashmap.end()) {
            (*it)->replace_by_new_value(value_hashmap[instruction_hash]);
            (*it)->clear_operands();
            it = block->get_instructions().erase(it);
        } else {
            value_hashmap[instruction_hash] = *it;
            ++it;
        }
    }
    for (const auto &child: cfg->dominance_children(func).at(block)) {
        run_on_block(func, child, value_hashmap);
    }
}

static void run_on_func(const FunctionPtr &func) {
    const auto &entry_block = func->get_blocks().front();
    std::unordered_map<std::string, InstructionPtr> value_hashmap;
    run_on_block(func, entry_block, value_hashmap);
}

namespace Pass {
void GlobalValueNumbering::transform(const std::shared_ptr<Module> module) {
    const auto constant_fold = create<ConstantFolding>();
    constant_fold->run_on(module);
    cfg = create<ControlFlowGraph>();
    cfg->run_on(module);
    // 不同的遍历顺序可能导致化简的结果不同
    // 跑多次GVN直到一个不动点
    for (const auto &func: *module) {
        run_on_func(func);
    }
    for (const auto &func: *module) {
        run_on_func(func);
    }
    cfg = nullptr;
    // GVN后可能出现一条指令被替换成其另一条指令，但是那条指令并不支配这条指令的users的问题
    // 可以通过 GCM 解决。在 GCM 中考虑value之间的依赖，会根据依赖将那条指令移动到正确的位置
    const auto &gcm = create<GlobalCodeMotion>();
    gcm->run_on(module);
}
}
