#include "Pass/Analysis.h"

namespace Pass {
void AliasAnalysis::run_on_func(const std::shared_ptr<Mir::Function> &func) {
    const auto func_result = std::make_shared<Result>();
    size_t alloc_id = 0;
    // 每个指针被赋予一组数字属性，如果两个指针的属性集合有"不相交"的属性对，则认为它们不可能指向同一内存位置
    size_t global_id = ++alloc_id;
    size_t stack_id = ++alloc_id;
    func_result->add_pair(global_id, stack_id);

    results.push_back(func_result);
}

void AliasAnalysis::analyze(const std::shared_ptr<const Mir::Module> module) {
    cfg = get_analysis_result<ControlFlowGraph>(module);
    for (const auto &func: *module) {
        run_on_func(func);
    }
}
}
