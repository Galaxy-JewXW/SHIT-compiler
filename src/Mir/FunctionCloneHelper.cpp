#include "Mir/FunctionCloneHelper.h"
#include "Pass/Analyses/ControlFlowGraph.h"

namespace Mir {
FunctionCloneHelper::FunctionCloneHelper() {
    cfg_info = Pass::get_analysis_result<Pass::ControlFlowGraph>(Module::instance());
}

std::shared_ptr<Value> FunctionCloneHelper::get_or_create(const std::shared_ptr<Value> &origin_value) {
    if (origin_value == nullptr) [[unlikely]] {
        return nullptr;
    }
    if (origin_value->is<Const>() != nullptr || origin_value->is<GlobalVariable>() != nullptr ||
        origin_value->is<Function>() != nullptr) [[unlikely]] {
        return origin_value;
    }
    if (value_map.count(origin_value)) [[unlikely]] {
        return value_map[origin_value];
    }
    if (origin_value->is<Block>() != nullptr) [[unlikely]] {
        return nullptr;
    }
    const auto inst{origin_value->as<Instruction>()};
    clone_instruction(inst, inst->get_block(), true);
    cloned_instructions.insert(inst);
    return value_map[inst];
}

void FunctionCloneHelper::clone_instruction(const std::shared_ptr<Instruction> &origin_instruction,
                                            const std::shared_ptr<Block> &parent_block, const bool lazy_cloning) {
    if (cloned_instructions.count(origin_instruction)) {
        const auto cloned_parent{get_or_create(parent_block)->as<Block>()};
        const auto cloned_inst{get_or_create(origin_instruction)->as<Instruction>()};
        cloned_inst->set_block(cloned_parent);
        return;
    }
    const auto cloned_inst{origin_instruction->clone(*this)};
    const auto cloned_parent{get_or_create(parent_block)->as<Block>()};
    cloned_inst->set_block(cloned_parent, false);
    if (!lazy_cloning) {
        cloned_parent->add_instruction(cloned_inst);
    }
    value_map.insert({origin_instruction, cloned_inst});
}

void FunctionCloneHelper::clone_block(const std::shared_ptr<Block> &origin_block) {
    visited_blocks.insert(origin_block);
    for (const auto &origin_inst: origin_block->get_instructions()) {
        clone_instruction(origin_inst, origin_block, false);
    }
    for (const auto &origin_child: cfg_info->graph(current_func).successors.at(origin_block)) {
        if (!visited_blocks.count(origin_child)) {
            clone_block(origin_child);
        }
    }
}

std::shared_ptr<Function> FunctionCloneHelper::clone_function(const std::shared_ptr<Function> &origin_func) {
    current_func = origin_func;
    auto cloned_function = std::make_shared<Function>(origin_func->get_name() + "_cloned",
                                                      origin_func->get_return_type());
    const auto &origin_arguments{origin_func->get_arguments()};
    for (size_t i{0}; i < origin_arguments.size(); ++i) {
        const auto cloned_arg = std::make_shared<Argument>("arg" + std::to_string(i),
                                                           origin_arguments[i]->get_type(), i);
        value_map.insert({origin_arguments[i], cloned_arg});
        cloned_function->add_argument(cloned_arg);
    }

    const auto &origin_blocks{origin_func->get_blocks()};
    for (size_t i{0}; i < origin_blocks.size(); ++i) {
        const auto cloned_block = Block::create("block" + std::to_string(i), cloned_function);
        value_map.insert({origin_blocks[i], cloned_block});
    }

    clone_block(origin_func->get_blocks().front());

    for (const auto &phi: phis) {
        const auto cloned_phi{get_or_create(phi)->as<Phi>()};
        for (const auto &[b, v]: phi->get_optional_values()) {
            cloned_phi->set_optional_value(get_or_create(b)->as<Block>(), get_or_create(v));
        }
    }

    cfg_info = nullptr;
    current_func = nullptr;
    return cloned_function;
}

#define HELPER_VALUE(v) (helper.get_or_create(v))
#define STD_STRING(s) (std::string{#s})

std::shared_ptr<Instruction> Alloc::clone(FunctionCloneHelper &helper) {
    return make_noinsert_instruction<Alloc>(STD_STRING(alloc), get_type()->as<Type::Pointer>()->get_contain_type());
}

std::shared_ptr<Instruction> Load::clone(FunctionCloneHelper &helper) {
    return make_noinsert_instruction<Load>(STD_STRING(load), HELPER_VALUE(get_addr()));
}

std::shared_ptr<Instruction> Store::clone(FunctionCloneHelper &helper) {
    return make_noinsert_instruction<Store>(HELPER_VALUE(get_addr()), HELPER_VALUE(get_value()));
}

std::shared_ptr<Instruction> GetElementPtr::clone(FunctionCloneHelper &helper) {
    std::vector<std::shared_ptr<Value>> indexes;
    for (size_t i = 1; i < operands_.size(); ++i) {
        indexes.push_back(HELPER_VALUE(operands_[i]));
    }
    return make_noinsert_instruction<GetElementPtr>(STD_STRING(gep), HELPER_VALUE(get_addr()), indexes);
}

std::shared_ptr<Instruction> BitCast::clone(FunctionCloneHelper &helper) {
    return make_noinsert_instruction<BitCast>(STD_STRING(bitcast), HELPER_VALUE(get_value()), get_type());
}

std::shared_ptr<Instruction> Fptosi::clone(FunctionCloneHelper &helper) {
    return make_noinsert_instruction<Fptosi>(STD_STRING(fptosi), HELPER_VALUE(get_value()));
}

std::shared_ptr<Instruction> Sitofp::clone(FunctionCloneHelper &helper) {
    return make_noinsert_instruction<Sitofp>(STD_STRING(sitofp), HELPER_VALUE(get_value()));
}

std::shared_ptr<Instruction> Fcmp::clone(FunctionCloneHelper &helper) {
    return make_noinsert_instruction<Fcmp>(STD_STRING(fcmp), fcmp_op(), HELPER_VALUE(get_lhs()),
                                           HELPER_VALUE(get_rhs()));
}

std::shared_ptr<Instruction> Icmp::clone(FunctionCloneHelper &helper) {
    return make_noinsert_instruction<Icmp>(STD_STRING(icmp), icmp_op(), HELPER_VALUE(get_lhs()),
                                           HELPER_VALUE(get_rhs()));
}

std::shared_ptr<Instruction> Zext::clone(FunctionCloneHelper &helper) {
    return make_noinsert_instruction<Zext>(STD_STRING(zext), HELPER_VALUE(get_value()));
}

std::shared_ptr<Instruction> Jump::clone(FunctionCloneHelper &helper) {
    return make_noinsert_instruction<Jump>(HELPER_VALUE(get_target_block())->as<Block>());
}

std::shared_ptr<Instruction> Branch::clone(FunctionCloneHelper &helper) {
    return make_noinsert_instruction<Branch>(HELPER_VALUE(get_cond()), HELPER_VALUE(get_true_block())->as<Block>(),
                                             HELPER_VALUE(get_false_block())->as<Block>());
}

std::shared_ptr<Instruction> Ret::clone(FunctionCloneHelper &helper) {
    if (operands_.empty()) {
        return make_noinsert_instruction<Ret>();
    }
    return make_noinsert_instruction<Ret>(HELPER_VALUE(get_value()));
}

std::shared_ptr<Instruction> Switch::clone(FunctionCloneHelper &helper) {
    const auto switch_{make_noinsert_instruction<Switch>(HELPER_VALUE(get_base()),
                                                         HELPER_VALUE(get_default_block())->as<Block>())};
    for (const auto &[v, b]: cases_table) {
        switch_->set_case(HELPER_VALUE(v)->as<Const>(), HELPER_VALUE(b)->as<Block>());
    }
    return switch_;
}

std::shared_ptr<Instruction> Call::clone(FunctionCloneHelper &helper) {
    std::vector<std::shared_ptr<Value>> params;
    const auto old_params{get_params()};
    for (const auto &p: old_params) {
        params.push_back(HELPER_VALUE(p));
    }
    if (get_type()->is_void()) {
        return create(HELPER_VALUE(get_function())->as<Function>(), params, nullptr, const_string_index);
    }
    return make_noinsert_instruction<Call>(STD_STRING(call), HELPER_VALUE(get_function())->as<Function>(), params);
}

std::shared_ptr<Instruction> FNeg::clone(FunctionCloneHelper &helper) {
    return make_noinsert_instruction<FNeg>(STD_STRING(fneg), HELPER_VALUE(get_value()));
}

std::shared_ptr<Instruction> Phi::clone(FunctionCloneHelper &helper) {
    static int i{0};
    const auto phi{create(STD_STRING(phi) + std::to_string(++i), get_type(), nullptr, {})};
    for (const auto &[b, v]: optional_values) {
        phi->optional_values[HELPER_VALUE(b)->as<Block>()] = nullptr;
        phi->add_operand(HELPER_VALUE(b)->as<Block>());
    }
    helper.get_phis().insert(shared_from_this()->as<Phi>());
    return phi;
}

std::shared_ptr<Instruction> Select::clone(FunctionCloneHelper &helper) {
    return make_noinsert_instruction<Select>(STD_STRING(select), HELPER_VALUE(get_cond()),
                                             HELPER_VALUE(get_true_value()), HELPER_VALUE(get_false_value()));
}

#define BINARY_CLONE(Class)                                                                                            \
std::shared_ptr<Instruction> Class::clone(FunctionCloneHelper &helper) {                                               \
    return make_noinsert_instruction<Class>(std::string{#Class}, HELPER_VALUE(get_lhs()),                              \
                                            HELPER_VALUE(get_rhs()));                                                  \
}

#define TERNARY_CLONE(Class)                                                                                           \
std::shared_ptr<Instruction> Class::clone(FunctionCloneHelper &helper) {                                               \
    return make_noinsert_instruction<Class>(std::string{#Class}, HELPER_VALUE(get_x()), HELPER_VALUE(get_y()),         \
                                            HELPER_VALUE(get_z()));                                                    \
}

BINARY_CLONE(Add)
BINARY_CLONE(Sub)
BINARY_CLONE(Mul)
BINARY_CLONE(Div)
BINARY_CLONE(Mod)
BINARY_CLONE(And)
BINARY_CLONE(Or)
BINARY_CLONE(Xor)
BINARY_CLONE(Smax)
BINARY_CLONE(Smin)
BINARY_CLONE(FAdd)
BINARY_CLONE(FSub)
BINARY_CLONE(FMul)
BINARY_CLONE(FDiv)
BINARY_CLONE(FMod)
BINARY_CLONE(FSmax)
BINARY_CLONE(FSmin)

TERNARY_CLONE(FMadd)
TERNARY_CLONE(FNmadd)
TERNARY_CLONE(FMsub)
TERNARY_CLONE(FNmsub)

#undef HELPER_VALUE
#undef STD_STRING
#undef BINARY_CLONE
#undef TERNARY_CLONE
} // namespace Mir
