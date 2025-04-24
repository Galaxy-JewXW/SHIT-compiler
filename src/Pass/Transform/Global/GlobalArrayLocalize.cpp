#include "Mir/Init.h"
#include "Pass/Transform.h"
using namespace Mir;

namespace {
// FIXME: 替换全局数组常量
[[maybe_unused]]
void replace_const_array_gv(const std::shared_ptr<Module> &module) {
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

bool array_can_localized(const std::shared_ptr<GlobalVariable> &gv) {
    std::vector<std::shared_ptr<Instruction>> alloca_users;
    for (const auto &user: gv->users()) {
        if (const auto inst = user->is<Instruction>()) {
            alloca_users.push_back(inst);
        } else {
            log_error("%s is not an instruction user of gv %s", user->to_string().c_str(), gv->to_string().c_str());
        }
    }
    for (size_t i = 0; i < alloca_users.size(); ++i) {
        const auto instruction = alloca_users[i];
        if (const auto op = instruction->get_op(); op == Operator::GEP) {
            const auto gep = instruction->as<GetElementPtr>();
            if (!gep->get_index()->is_constant()) {
                return false;
            }
            for (const auto &user: gep->users()) {
                alloca_users.push_back(user->as<Instruction>());
            }
        } else if (op == Operator::BITCAST) {
            for (const auto &user: instruction->users()) {
                alloca_users.push_back(user->as<Instruction>());
            }
        } else if (op == Operator::CALL) {
            if (const auto &func_name = instruction->as<Call>()->get_function()->get_name();
                func_name.find("llvm.memset") == std::string::npos) {
                return false;
            }
        }
    }
    return true;
}

// TODO: 非 main 函数的情况下将全局变量作为参数进行传递
void localize(const std::shared_ptr<Module> &module) {
    const auto func_analysis = Pass::get_analysis_result<Pass::FunctionAnalysis>(module);
    std::unordered_set<std::shared_ptr<GlobalVariable>> can_replaced;
    for (const auto &gv: module->get_global_variables()) {
        if (const auto gv_type = gv->get_type()->as<Type::Pointer>()->get_contain_type();
            gv_type->is_array()) {
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
        if (!array_can_localized(gv)) {
            continue;
        }
        const auto new_entry = Block::create(Builder::gen_block_name()),
                   current_entry = func->get_blocks().front();
        new_entry->set_function(func, false);
        func->get_blocks().insert(func->get_blocks().begin(), new_entry);
        const auto new_alloc = Alloc::create(Builder::gen_variable_name(),
                                             gv->get_type()->as<Type::Pointer>()->get_contain_type(), new_entry);
        std::vector<int> indexes;
        auto type = gv->get_type()->as<Type::Pointer>()->get_contain_type();
        while (type->is_array()) {
            indexes.push_back(static_cast<int>(type->as<Type::Array>()->get_size()));
            type = type->as<Type::Array>()->get_element_type();
        }
        const auto array_init = gv->get_init_value()->as<Init::Array>();
        array_init->gen_store_inst(new_alloc, new_entry, indexes);
        gv->replace_by_new_value(new_alloc);
        Jump::create(current_entry, new_entry);
        Pass::get_analysis_result<Pass::ControlFlowGraph>(module)->set_dirty(func);
        Pass::get_analysis_result<Pass::DominanceGraph>(module)->set_dirty(func);
    }
    if (!can_replaced.empty()) {
        for (auto it = module->get_global_variables().begin(); it != module->get_global_variables().end();) {
            if (can_replaced.find(*it) == can_replaced.end()) {
                ++it;
            } else {
                it = module->get_global_variables().erase(it);
            }
        }
    }
}
}

namespace Pass {
void GlobalArrayLocalize::transform(const std::shared_ptr<Module> module) {
    localize(module);
}
}
