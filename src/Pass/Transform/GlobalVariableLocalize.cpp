#include "Mir/Init.h"
#include "Pass/Transform.h"

namespace Pass {
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

    // FIXME: 替换全局数组常量
    [[maybe_unused]] void replace_const_array_gv(const std::shared_ptr<Module> &module) {
        std::vector<std::shared_ptr<GlobalVariable>> can_replaced;
        for (const auto &gv: module->get_global_variables()) {
            if (const auto gv_type = gv->get_type()->as<Type::Pointer>()->get_contain_type();
                gv_type->is_array() && gv->is_constant_gv()) {
                can_replaced.push_back(gv);
            }
        }
        for (const auto &gv: can_replaced) {
            std::vector<int> indexes;
            const auto &array_initial = gv->get_init_value()->as<Init::Array>();
            auto do_replace = [&](auto &&self, const std::shared_ptr<GetElementPtr> &gep) {
                const auto &gep_idx = gep->get_index();
                if (!gep_idx->is_constant()) {
                    return;
                }
                indexes.push_back(**gep_idx->as<ConstInt>());
                for (const auto &user: gep->users()) {
                    if (const auto load = std::dynamic_pointer_cast<Load>(user)) {
                        const auto initial = array_initial->get_init_value(indexes);
                        const auto value = initial->as<Init::Constant>()->get_const_value();
                        load->replace_by_new_value(value);
                    } else if (const auto cur_gep = std::dynamic_pointer_cast<GetElementPtr>(user)) {
                        self(self, cur_gep);
                    }
                }
                indexes.pop_back();
            };
            for (const auto &user: gv->users()) {
                const auto gep = std::dynamic_pointer_cast<GetElementPtr>(user);
                if (gep == nullptr) { continue; }
                do_replace(do_replace, gep);
            }
        }
    }

    void localize(const std::shared_ptr<Module> &module) {
        const auto func_analysis = Pass::create<FunctionAnalysis>();
        func_analysis->run_on(module);
        std::vector<std::shared_ptr<GlobalVariable>> can_replaced;
        for (const auto &gv: module->get_global_variables()) {
            if (const auto gv_type = gv->get_type()->as<Type::Pointer>()->get_contain_type();
                !gv_type->is_array()) {
                can_replaced.push_back(gv);
            }
        }
        for (const auto &gv: can_replaced) {
            std::unordered_set<std::shared_ptr<Function>> use_function;
            for (const auto &user: gv->users()) {
                if (const auto inst = std::dynamic_pointer_cast<Instruction>(user)) {
                    use_function.insert(inst->get_block()->get_function());
                }
            }
            if (use_function.size() != 1) { continue; }
            const auto &func = *use_function.begin();
            if (func_analysis->func_info(func).is_recursive) { continue; }
            const auto &entry = func->get_blocks().front();
            const auto new_alloc = Alloc::create(Builder::gen_variable_name(),
                                                 gv->get_type()->as<Type::Pointer>()->get_contain_type(), nullptr);
            const auto new_store = Store::create(new_alloc,
                                                 gv->get_init_value()->as<Init::Constant>()->get_const_value(),
                                                 nullptr);
            new_alloc->set_block(entry, false);
            new_store->set_block(entry, false);
            entry->get_instructions().insert(entry->get_instructions().begin(), new_store);
            entry->get_instructions().insert(entry->get_instructions().begin(), new_alloc);
            gv->replace_by_new_value(new_alloc);
        }
        if (!can_replaced.empty()) {
            Pass::create<Mem2Reg>()->run_on(module);
            Pass::create<ConstantFolding>()->run_on(module);
        }
    }
}

void GlobalVariableLocalize::transform(const std::shared_ptr<Module> module) {
    replace_const_normal_gv(module);
    // replace_const_array_gv(module);
    localize(module);
}
}
