#include "Pass/Util.h"
#include "Pass/Transforms/Array.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
size_t size_of_type(const std::shared_ptr<Type::Type> &type) {
    if (type->is_integer() || type->is_float()) {
        return 1;
    }
    if (type->is_array()) {
        return type->as<Type::Array>()->get_flattened_size();
    }
    log_error("invalid type: %s", type->to_string().c_str());
}

bool is_folded_leaf_gep(const std::shared_ptr<Instruction> &instruction) {
    const auto gep = instruction->is<GetElementPtr>();
    if (gep == nullptr) {
        return false;
    }
    const bool is_folded = gep->get_addr()->is<GetElementPtr>() != nullptr;
    return is_folded;
}

void try_fold_gep(const std::shared_ptr<GetElementPtr> &gep) {
    if (gep->users().size() == 0) {
        return;
    }
    const auto current_block = gep->get_block();
    std::vector<std::shared_ptr<GetElementPtr>> chain;
    std::shared_ptr<Value> current = gep;
    while (const auto cur_gep = current->is<GetElementPtr>()) {
        chain.push_back(cur_gep);
        current = cur_gep->get_addr();
    }
    std::reverse(chain.begin(), chain.end());
    size_t zero_count = 0;
    for (const auto &chain_gep: chain) {
        if (chain_gep->get_operands().size() > 2) {
            ++zero_count;
        }
    }
    std::vector<std::shared_ptr<Value>> offsets;
    offsets.reserve(zero_count);
    for (size_t i = 0; i < zero_count; ++i) {
        offsets.push_back(ConstInt::create(0));
    }
    std::shared_ptr<Value> offset = nullptr;
    for (const auto &chain_gep: chain) {
        const auto base_type = chain_gep->get_type()->as<Type::Pointer>()->get_contain_type();
        const auto size = size_of_type(base_type);
        const auto const_int = ConstInt::create(static_cast<int>(size));
        const auto new_mul = Mul::create("mul", const_int, chain_gep->get_index(), current_block);
        Pass::Utils::move_instruction_before(new_mul, gep);
        if (offset == nullptr) {
            offset = new_mul;
        } else {
            const auto new_add = Add::create("add", offset, new_mul, current_block);
            Pass::Utils::move_instruction_before(new_add, gep);
            offset = new_add;
        }
    }
    offsets.push_back(offset);
    if (const auto new_gep = GetElementPtr::create("gep", chain.front()->get_addr(), offsets, current_block);
        new_gep != gep->get_addr()) {
        const auto new_inst = new_gep->as<GetElementPtr>();
        Pass::Utils::move_instruction_before(new_inst, gep);
        gep->replace_by_new_value(new_inst);
    }
}
}

namespace Pass {
void GepFolding::run_on_func(const std::shared_ptr<Function> &func) const {
    std::vector<std::shared_ptr<GetElementPtr>> geps;
    for (const auto &block: dom_graph->dom_tree_layer(func)) {
        for (const auto &instruction: block->get_instructions()) {
            if (is_folded_leaf_gep(instruction)) {
                geps.push_back(instruction->as<GetElementPtr>());
            }
        }
    }
    std::reverse(geps.begin(), geps.end());
    for (const auto &gep: geps) {
        try_fold_gep(gep);
    }
}


void GepFolding::transform(const std::shared_ptr<Module> module) {
    dom_graph = get_analysis_result<DominanceGraph>(module);
    for (const auto &func: *module) {
        run_on_func(func);
    }
    module->update_id();
    dom_graph = nullptr;
    create<GlobalValueNumbering>()->run_on(module);
}
}
