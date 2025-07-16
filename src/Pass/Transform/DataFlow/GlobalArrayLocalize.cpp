#include "Mir/Init.h"
#include "Pass/Transforms/Array.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
void replace_const_array_gv(const std::shared_ptr<Module> &module) {
    std::vector<std::shared_ptr<GlobalVariable>> can_replaced;
    for (const auto &gv: module->get_global_variables()) {
        if (const auto gv_type = gv->get_type()->as<Type::Pointer>()->get_contain_type();
            gv_type->is_array() && gv->is_constant_gv()) {
            can_replaced.push_back(gv);
        }
    }
    for (const auto &gv: can_replaced) {
        const auto array_type = gv->get_type()->as<Type::Pointer>()->get_contain_type();
        std::vector<int> array_dimensions;
        std::shared_ptr<Type::Type> _type = array_type;
        while (_type->is_array()) {
            array_dimensions.push_back(static_cast<int>(_type->as<Type::Array>()->get_size()));
            _type = _type->as<Type::Array>()->get_element_type();
        }

        std::vector<int> array_strides;
        const auto l = array_dimensions.size();
        array_strides.reserve(l);
        array_strides[l - 1] = 1;
        for (int i = static_cast<int>(l) - 2; i >= 0; --i) {
            array_strides[i] = array_strides[i + 1] * array_dimensions[i + 1];
        }

        const auto array_initial = gv->get_init_value()->as<Init::Array>();
        for (const auto &gv_user: gv->users()) {
            const auto gep{gv_user->is<GetElementPtr>()};
            if (gep == nullptr || !gep->get_index()->is_constant()) {
                continue;
            }
            int offset{**gep->get_index()->as<ConstInt>()};
            std::vector<int> indexes;
            indexes.reserve(array_strides.size());
            for (const auto &stride: array_strides) {
                indexes.push_back(offset / stride);
                offset %= stride;
            }
            if (offset != 0) [[unlikely]] {
                log_error("%s", "offset is not zero");
            }
            const auto constant_initial = array_initial->get_init_value(indexes)->as<Init::Constant>();
            const auto constant_value = constant_initial->get_const_value();
            for (const auto &_load: gep->users()) {
                if (const auto load = _load->is<Load>()) {
                    load->replace_by_new_value(constant_value);
                }
            }
        }
    }
}

bool array_can_localized(const std::shared_ptr<GlobalVariable> &gv) {
    std::vector<std::shared_ptr<Instruction>> worklist;
    std::unordered_set<std::shared_ptr<Instruction>> visited;

    for (const auto &user: gv->users()) {
        if (const auto inst = user->is<Instruction>()) [[likely]] {
            worklist.push_back(inst);
            visited.insert(inst);
        } else {
            log_error("%s is not an instruction user of gv %s", user->to_string().c_str(), gv->to_string().c_str());
        }
    }
    while (!worklist.empty()) {
        const auto instruction = worklist.back();
        worklist.pop_back();
        if (const auto op = instruction->get_op(); op == Operator::GEP) {
            const auto gep = instruction->as<GetElementPtr>();
            if (!gep->get_index()->is_constant()) {
                return false;
            }
            for (const auto &user: gep->users()) {
                if (const auto inst_user = user->as<Instruction>(); visited.insert(inst_user).second) {
                    worklist.push_back(inst_user);
                }
            }
        } else if (op == Operator::BITCAST) {
            for (const auto &user: instruction->users()) {
                if (const auto inst_user = user->as<Instruction>(); visited.insert(inst_user).second) {
                    worklist.push_back(inst_user);
                }
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

void localize(const std::shared_ptr<Module> &module) {
    const auto func_analysis = Pass::get_analysis_result<Pass::FunctionAnalysis>(module);
    std::unordered_set<std::shared_ptr<GlobalVariable>> can_replace, replaced;
    for (const auto &gv: module->get_global_variables()) {
        if (const auto gv_type = gv->get_type()->as<Type::Pointer>()->get_contain_type(); gv_type->is_array()) {
            can_replace.insert(gv);
        }
    }
    for (const auto &gv: can_replace) {
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
        const auto new_entry = Block::create(Builder::gen_block_name()), current_entry = func->get_blocks().front();
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
        Pass::set_analysis_result_dirty<Pass::ControlFlowGraph>(func);
        replaced.insert(gv);
    }
    if (!replaced.empty()) {
        for (auto it = module->get_global_variables().begin(); it != module->get_global_variables().end();) {
            if (replaced.find(*it) == replaced.end()) {
                ++it;
            } else {
                it = module->get_global_variables().erase(it);
            }
        }
    }
}
} // namespace

namespace Pass {
void GlobalArrayLocalize::transform(const std::shared_ptr<Module> module) {
    create<GepFolding>()->run_on(module);
    replace_const_array_gv(module);
    localize(module);
}
} // namespace Pass
