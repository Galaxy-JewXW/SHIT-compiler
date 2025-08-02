#include "Mir/Init.h"
#include "Pass/Transform.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
// Replaces loads from simple, constant global variables with their initial values.
void replace_const_normal_gv(const std::shared_ptr<Module> &module) {
    std::vector<std::shared_ptr<GlobalVariable>> can_replaced;
    for (const auto &gv: module->get_global_variables()) {
        // Check if the global variable is a constant and not an array.
        if (const auto gv_type = gv->get_type()->as<Type::Pointer>()->get_contain_type();
            !gv_type->is_array() && gv->is_constant_gv()) {
            can_replaced.push_back(gv);
        }
    }
    // Replace all loads of these constant globals with the constant value itself.
    for (const auto &gv: can_replaced) {
        for (const auto &user: gv->users()) {
            if (const auto load = std::dynamic_pointer_cast<Load>(user)) {
                const auto init = gv->get_init_value();
                load->replace_by_new_value(init->as<Init::Constant>()->get_const_value());
            }
        }
    }
}

// Attempts to convert global variables into local variables.
void localize(const std::shared_ptr<Module> &module) {
    const auto func_analysis = Pass::get_analysis_result<Pass::FunctionAnalysis>(module);
    std::unordered_set<std::shared_ptr<GlobalVariable>> can_replaced;
    // Find all non-array global variables that could potentially be localized.
    for (const auto &gv: module->get_global_variables()) {
        if (const auto gv_type = gv->get_type()->as<Type::Pointer>()->get_contain_type(); !gv_type->is_array()) {
            can_replaced.insert(gv);
        }
    }

    for (const auto &gv: can_replaced) {
        std::unordered_set<std::shared_ptr<Function>> use_function;
        // Find all functions that use this global variable.
        for (const auto &user: gv->users()) {
            if (const auto inst = std::dynamic_pointer_cast<Instruction>(user)) {
                use_function.insert(inst->get_block()->get_function());
            }
        }

        // The transformation is only valid if the global is used by exactly one function.
        if (use_function.size() != 1) {
            continue;
        }

        const auto &func = *use_function.begin();

        // Only perform localization if the sole user function is `main`.
        // This prevents incorrectly localizing a global whose state must persist
        // across calls to a non-main function.
        if (func->get_name() != "main") {
            continue;
        }

        // Do not localize if the function is recursive.
        if (func_analysis->func_info(func).is_recursive) {
            continue;
        }

        // If all conditions are met, create a local variable (alloca) in the function's entry block.
        const auto &entry = func->get_blocks().front();
        const auto new_alloc = Alloc::create(Builder::gen_variable_name(),
                                             gv->get_type()->as<Type::Pointer>()->get_contain_type(), nullptr);

        // Initialize the new local variable with the global's initial value.
        const auto new_store =
                Store::create(new_alloc, gv->get_init_value()->as<Init::Constant>()->get_const_value(), nullptr);

        new_alloc->set_block(entry, false);
        new_store->set_block(entry, false);

        // Insert the new alloca and store at the beginning of the entry block.
        entry->get_instructions().insert(entry->get_instructions().begin(), new_store);
        entry->get_instructions().insert(entry->get_instructions().begin(), new_alloc);

        // Replace all uses of the global variable with the new local variable.
        gv->replace_by_new_value(new_alloc);
    }

    // Clean up any global variables that are no longer used.
    const auto origin_size = module->get_global_variables().size();
    for (auto it = module->get_global_variables().begin(); it != module->get_global_variables().end();) {
        if ((*it)->users().size() == 0) {
            it = module->get_global_variables().erase(it);
        } else {
            ++it;
        }
    }

    // If we removed globals and created allocas, run Mem2Reg to promote the allocas to registers.
    if (origin_size != module->get_global_variables().size()) {
        Pass::Pass::create<Pass::Mem2Reg>()->run_on(module);
    }
}
} // namespace

namespace Pass {
void GlobalVariableLocalize::transform(const std::shared_ptr<Module> module) {
    replace_const_normal_gv(module);
    localize(module);
}
} // namespace Pass
