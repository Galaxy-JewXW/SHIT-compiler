#include "Mir/Instruction.h"
#include "Pass/Analysis.h"

namespace {
size_t gen_alloc_id() {
    static size_t alloc_id = 0;
    return ++alloc_id;
}
}

namespace Pass {
void AliasAnalysis::run_on_func(const std::shared_ptr<Mir::Function> &func) {
    const auto func_alias_result = std::make_shared<Result>();
    // 每个指针被赋予一组数字属性，如果两个指针的属性集合有"不相交"的属性对，则认为它们不可能指向同一内存位置
    size_t global_id = gen_alloc_id();
    size_t stack_id = gen_alloc_id();
    func_alias_result->add_pair(global_id, stack_id);

    std::unordered_set<size_t> global_groups, stack_groups;
    std::unordered_set<InheritEdge, InheritEdge::Hasher> inherit_graph;

    for (const auto &gv: module->get_global_variables()) {
        auto gv_id = gen_alloc_id();
        func_alias_result->add_value(gv, {global_id, gv_id});
        global_groups.insert(gv_id);
    }

    auto arg_id = gen_alloc_id();
    for (const auto &arg: func->get_arguments()) {
        if (arg->get_type()->is_pointer()) {
            func_alias_result->add_value(arg, {arg_id});
        }
    }

    const auto &dom_tree_layer_order = cfg->dom_tree_layer(func);
    for (const auto &block: dom_tree_layer_order) {
        for (const auto &inst: block->get_instructions()) {
            if (!inst->get_type()->is_pointer()) {
                continue;
            }
            if (const auto op = inst->get_op(); op == Mir::Operator::ALLOC) {
                auto id = gen_alloc_id();
                stack_groups.insert(id);
                func_alias_result->add_pair(id, arg_id);
                func_alias_result->add_value(inst, {stack_id, id});
            }
        }
    }

    results.push_back(func_alias_result);
}

void AliasAnalysis::analyze(const std::shared_ptr<const Mir::Module> module) {
    this->module = std::const_pointer_cast<Mir::Module>(module);
    cfg = get_analysis_result<ControlFlowGraph>(module);
    for (const auto &func: *module) {
        run_on_func(func);
    }
}
}
