#include "Pass/Transforms/Array.h"
#include "Pass/Util.h"

using namespace Mir;

namespace {
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
} // namespace

namespace Pass {
void LoadEliminate::handle_load(const std::shared_ptr<Load> &load) {
    // 获取基础地址
    std::shared_ptr<Value> addr = load->get_addr();
    while (addr->is<BitCast>()) {
        addr = addr->as<BitCast>()->get_value();
    }
    if (addr->is<Load>()) {
        return;
    }
    if (const auto gep = addr->is<GetElementPtr>()) {
        addr = gep->get_addr();
        const auto &array_store =
                store_indexes.try_emplace(addr, std::unordered_map<ValuePtr, ValuePtr>{}).first->second;
        auto &array_load = load_indexes.try_emplace(addr, std::unordered_map<ValuePtr, ValuePtr>{}).first->second;
        if (const auto index = gep->get_index(); array_store.count(index)) {
            // 直接替换为 store 的值
            load->replace_by_new_value(array_store.at(index));
            deleted_instructions.insert(load);
        } else if (array_load.count(index)) {
            // 复用上一次 load 的结果
            load->replace_by_new_value(array_load.at(index));
            deleted_instructions.insert(load);
        } else {
            // 记录当前 load 的结果
            array_load.emplace(index, load);
            load_indexes[addr] = array_load;
        }
    } else if (const auto gv = addr->is<GlobalVariable>()) {
        if (store_global.count(gv)) {
            load->replace_by_new_value(store_global.at(gv));
            deleted_instructions.insert(load);
        } else if (load_global.count(gv)) {
            load->replace_by_new_value(load_global.at(gv));
            deleted_instructions.insert(load);
        } else {
            load_global[gv] = load;
        }
    }
}

void LoadEliminate::handle_store(const std::shared_ptr<Store> &store) {
    // 获取基础地址
    std::shared_ptr<Value> addr = store->get_addr();
    while (addr->is<BitCast>()) {
        addr = addr->as<BitCast>()->get_value();
    }
    if (addr->is<Load>()) {
        return;
    }
    if (const auto gep = addr->is<GetElementPtr>()) {
        addr = gep->get_addr();
        if (const auto index = gep->get_index(); index->is_constant()) {
            // 常量索引：更新 store 映射，清除对应的 load 映射
            store_indexes.try_emplace(addr, std::unordered_map<ValuePtr, ValuePtr>{});
            store_indexes[addr][index] = store->get_value();
            load_indexes.try_emplace(addr, std::unordered_map<ValuePtr, ValuePtr>{});
            load_indexes[addr].erase(index);
        } else {
            // 变量索引：
            store_indexes.try_emplace(addr, std::unordered_map<ValuePtr, ValuePtr>{});
            store_indexes[addr].clear();
            store_indexes[addr][index] = store->get_value();
            load_indexes[addr] = std::unordered_map<ValuePtr, ValuePtr>{};
        }
    } else {
        const auto gv = addr->as<GlobalVariable>();
        store_global[gv] = store->get_value();
        load_global.erase(gv);
    }
}

void LoadEliminate::handle_call(const std::shared_ptr<Call> &call) {
    const auto called_function = call->get_function()->as<Function>();
    if (called_function->is_sysy_runtime_func()) {
        if (called_function->get_name().find("put") != std::string::npos ||
            called_function->get_name().find("time") != std::string::npos) {
            return;
        }
    }
    bool has_side_effect{false}, memory_write{false};
    std::unordered_set<std::shared_ptr<GlobalVariable>> used_global_variables;
    if (called_function->is_runtime_func()) {
        has_side_effect = true;
        memory_write = true;
    } else {
        const auto &info = function_analysis->func_info(called_function);
        has_side_effect = info.has_side_effect;
        memory_write = info.memory_write;
        used_global_variables = info.used_global_variables;
    }
    if (has_side_effect) {
        for (const auto &param: call->get_params()) {
            if (param->get_type()->is_pointer()) {
                const auto base = base_addr(param);
                load_indexes.erase(base);
                store_indexes.erase(base);
            }
        }
    }
    if (memory_write) {
        for (const auto &used_gv: used_global_variables) {
            if (used_gv->get_type()->as<Type::Pointer>()->get_contain_type()->is_array()) {
                load_indexes.erase(used_gv);
                store_indexes.erase(used_gv);
            } else {
                load_global.erase(used_gv);
                store_global.erase(used_gv);
            }
        }
    }
}

void LoadEliminate::dfs(const std::shared_ptr<Block> &block) {
    // 在进入基本块前克隆当前状态（存储和加载映射），处理完子块后恢复状态
    const auto cur_load_indexes = load_indexes, cur_store_indexes = store_indexes;
    const auto cur_load_global = load_global, cur_store_global = store_global;
    // 控制流合并可能引入不确定性，基本块有多个前驱时清空状态
    try {
        if (cfg_info->graph(block->get_function()).predecessors.at(block).size() > 1) {
            clear();
        }
    } catch (const std::out_of_range &) {
        log_error("%s", block->get_function()->to_string().c_str());
    }
    for (const auto &instruction: block->get_instructions()) {
        if (const auto op = instruction->get_op(); op == Operator::LOAD) {
            // 检查冗余并标记删除
            handle_load(instruction->as<Load>());
        } else if (op == Operator::STORE) {
            // 更新存储状态
            handle_store(instruction->as<Store>());
        } else if (op == Operator::CALL) {
            // 函数调用可能造成影响
            handle_call(instruction->as<Call>());
        }
    }
    for (const auto &child: dom_info->graph(block->get_function()).dominance_children.at(block)) {
        dfs(child);
    }
    load_indexes = cur_load_indexes;
    store_indexes = cur_store_indexes;
    load_global = cur_load_global;
    store_global = cur_store_global;
}

void LoadEliminate::run_on_func(const std::shared_ptr<Function> &func) {
    clear();
    dfs(func->get_blocks().front());
}

void LoadEliminate::transform(const std::shared_ptr<Module> module) {
    deleted_instructions.clear();
    cfg_info = get_analysis_result<ControlFlowGraph>(module);
    dom_info = get_analysis_result<DominanceGraph>(module);
    function_analysis = get_analysis_result<FunctionAnalysis>(module);
    for (const auto &function: *module) {
        run_on_func(function);
    }
    Utils::delete_instruction_set(module, deleted_instructions);
    cfg_info = nullptr;
    dom_info = nullptr;
    function_analysis = nullptr;
    deleted_instructions.clear();
}

void LoadEliminate::transform(const std::shared_ptr<Function> &func) {
    deleted_instructions.clear();
    cfg_info = get_analysis_result<ControlFlowGraph>(Module::instance());
    dom_info = get_analysis_result<DominanceGraph>(Module::instance());
    function_analysis = get_analysis_result<FunctionAnalysis>(Module::instance());
    run_on_func(func);
    Utils::delete_instruction_set(Module::instance(), deleted_instructions);
    cfg_info = nullptr;
    dom_info = nullptr;
    function_analysis = nullptr;
    deleted_instructions.clear();
}
} // namespace Pass
