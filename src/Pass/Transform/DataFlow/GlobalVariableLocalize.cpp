#include "Mir/Init.h"
#include "Pass/Transform.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
// 替换普通的全局常量
void replace_const_normal_gv(const std::shared_ptr<Module> &module) {
    std::vector<std::shared_ptr<GlobalVariable>> can_replaced;
    for (const auto &gv: module->get_global_variables()) {
        if (const auto gv_type = gv->get_type()->as<Type::Pointer>()->get_contain_type();
            !gv_type->is_array() && gv->is_constant_gv()) {
            can_replaced.push_back(gv);
        }
    }
    for (const auto &gv: can_replaced) {
        for (const auto &user: gv->users()) {
            if (const auto load = std::dynamic_pointer_cast<Load>(user)) {
                const auto init = gv->get_init_value();
                load->replace_by_new_value(init->as<Init::Constant>()->get_const_value());
            }
        }
    }
}

// TODO: 非 main 函数的情况下将全局变量作为参数进行传递
void localize(const std::shared_ptr<Module> &module) {
    const auto func_analysis = Pass::get_analysis_result<Pass::FunctionAnalysis>(module);
    std::unordered_set<std::shared_ptr<GlobalVariable>> can_replaced;
    for (const auto &gv: module->get_global_variables()) {
        if (const auto gv_type = gv->get_type()->as<Type::Pointer>()->get_contain_type(); !gv_type->is_array()) {
            can_replaced.insert(gv);
        }
    }
    for (const auto &gv: can_replaced) {
        std::unordered_set<std::shared_ptr<Function>> use_function;
        for (const auto &user: gv->users()) {
            if (const auto inst = std::dynamic_pointer_cast<Instruction>(user)) {
                use_function.insert(inst->get_block()->get_function());
            }
        }
        if (use_function.size() != 1) {
            continue;
        }
        const auto &func = *use_function.begin();
        if (func->get_name() != "main") {
            continue;
        }
        if (func_analysis->func_info(func).is_recursive) {
            continue;
        }
        const auto &entry = func->get_blocks().front();
        const auto new_alloc = Alloc::create(Builder::gen_variable_name(),
                                             gv->get_type()->as<Type::Pointer>()->get_contain_type(), nullptr);
        const auto new_store =
                Store::create(new_alloc, gv->get_init_value()->as<Init::Constant>()->get_const_value(), nullptr);
        new_alloc->set_block(entry, false);
        new_store->set_block(entry, false);
        entry->get_instructions().insert(entry->get_instructions().begin(), new_store);
        entry->get_instructions().insert(entry->get_instructions().begin(), new_alloc);
        gv->replace_by_new_value(new_alloc);
    }
    const auto origin_size = module->get_global_variables().size();
    for (auto it = module->get_global_variables().begin(); it != module->get_global_variables().end();) {
        if ((*it)->users().size() == 0) {
            it = module->get_global_variables().erase(it);
        } else {
            ++it;
        }
    }
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
