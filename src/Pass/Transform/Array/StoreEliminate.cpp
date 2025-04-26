#include "Pass/Util.h"
#include "Pass/Transforms/Array.h"

using namespace Mir;

namespace {
std::shared_ptr<Module> module{nullptr};

std::shared_ptr<Value> base_addr(const std::shared_ptr<Value> &inst) {
    auto ret = inst;
    while (ret->is<BitCast>() != nullptr || ret->is<GetElementPtr>() != nullptr) {
        if (const auto bitcast = ret->is<BitCast>()) {
            ret = bitcast->get_value();
        } else if (const auto gep = ret->is<GetElementPtr>()) {
            ret = gep->get_addr();
        }
    }
    return ret;
}
}

namespace Pass {
void StoreEliminate::handle_load(const std::shared_ptr<Load> &load) {
    std::shared_ptr<Value> addr = load->get_addr();
    while (addr->is<BitCast>()) {
        addr = addr->as<BitCast>()->get_value();
    }
    if (addr->is<Load>()) {
        return;
    }
    if (const auto gv = addr->is<GlobalVariable>()) {
        store_global.erase(gv);
    } else if (const auto gep = addr->is<GetElementPtr>()) {
        const auto &base_addr = gep->get_addr(),
                   &index = gep->get_index();
        store_map.try_emplace(base_addr, std::unordered_map<ValuePtr, std::shared_ptr<Store>>{});
        if (index->is_constant()) {
            store_map[base_addr].erase(index);
        } else {
            store_map[base_addr].clear();
        }
    }
}

void StoreEliminate::handle_store(const std::shared_ptr<Store> &store) {
    std::shared_ptr<Value> addr = store->get_addr();
    while (addr->is<BitCast>()) {
        addr = addr->as<BitCast>()->get_value();
    }
    if (addr->is<Load>()) {
        return;
    }
    if (const auto gv = addr->is<GlobalVariable>()) {
        if (store_global.count(gv)) {
            deleted_instructions.insert(store_global[gv]);
        }
        store_global[gv] = store;
    } else if (const auto gep = addr->is<GetElementPtr>()) {
        const auto &base_addr = gep->get_addr(),
                   &index = gep->get_index();
        store_map.try_emplace(base_addr, std::unordered_map<ValuePtr, std::shared_ptr<Store>>{});
        if (index->is_constant()) {
            if (store_map[base_addr].count(index)) {
                deleted_instructions.insert(store_map[base_addr][index]);
            }
            store_map[base_addr][index] = store;
        } else {
            if (store_map[base_addr].count(index)) {
                deleted_instructions.insert(store_map[base_addr][index]);
            }
            store_map[base_addr].clear();
            store_map[base_addr][index] = store;
        }
    }
}

void StoreEliminate::handle_call(const std::shared_ptr<Call> &call) {
    const auto called_function = call->get_function()->as<Function>();
    if (called_function->is_sysy_runtime_func()) {
        if (called_function->get_name().find("put") != std::string::npos ||
            called_function->get_name().find("time") != std::string::npos) {
            return;
        }
    }
    bool has_side_effect{false}, memory_write{false};
    if (called_function->is_runtime_func()) {
        has_side_effect = true;
        memory_write = true;
    } else {
        const auto &info = function_analysis->func_info(called_function);
        has_side_effect = info.has_side_effect;
        memory_write = info.memory_write;
    }
    if (has_side_effect) {
        for (const auto &param: call->get_params()) {
            if (param->get_type()->is_pointer()) {
                const auto base = base_addr(param);
                store_map.erase(base);
            }
        }
    }
    if (memory_write) {
        store_global.clear();
        for (auto it = store_map.begin(); it != store_map.end();) {
            if (it->first->is<GlobalVariable>()) {
                it = store_map.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void StoreEliminate::run_on_func(const std::shared_ptr<Function> &func) {
    for (const auto &block: func->get_blocks()) {
        clear();
        deleted_instructions.clear();
        for (const auto &instruction: block->get_instructions()) {
            if (const auto op = instruction->get_op();
                op == Operator::LOAD) {
                handle_load(instruction->as<Load>());
            } else if (op == Operator::STORE) {
                handle_store(instruction->as<Store>());
            } else if (op == Operator::CALL) {
                handle_call(instruction->as<Call>());
            }
        }
        Utils::delete_instruction_set(module, deleted_instructions);
    }
}

void StoreEliminate::transform(const std::shared_ptr<Module> module) {
    ::module = module;
    deleted_instructions.clear();
    function_analysis = get_analysis_result<FunctionAnalysis>(module);
    for (const auto &function: *module) {
        run_on_func(function);
    }
    function_analysis = nullptr;
    deleted_instructions.clear();
    ::module = nullptr;
}
}
