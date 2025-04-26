#include <functional>

#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Analyses/DominanceGraph.h"
#include "Pass/Transforms/DCE.h"

using namespace Mir;

using FunctionPtr = std::shared_ptr<Function>;
using FunctionMap = std::unordered_map<FunctionPtr, std::unordered_set<FunctionPtr>>;
using FunctionSet = std::unordered_set<FunctionPtr>;

namespace Pass {
void DeadFuncEliminate::transform(const std::shared_ptr<Module> module) {
    const auto func_analysis = create<FunctionAnalysis>();
    func_analysis->run_on(module);
    const auto main_func = module->get_main_function();
    FunctionSet reachable_functions;
    std::function<void(const FunctionPtr &, FunctionSet &)> dfs =
            [&](const FunctionPtr &cur_function, FunctionSet &reachable) {
        if (reachable.find(cur_function) != reachable.end()) {
            return;
        }
        reachable.insert(cur_function);
        const auto &call_graph = func_analysis->call_graph_func(cur_function);
        for (const auto &func: call_graph) {
            dfs(func, reachable);
        }
    };
    dfs(main_func, reachable_functions);
    for (auto it = module->get_functions().begin(); it != module->get_functions().end();) {
        if (reachable_functions.find(*it) == reachable_functions.end()) {
            const auto func = *it;
            for (const auto &block: func->get_blocks()) {
                std::for_each(block->get_instructions().begin(), block->get_instructions().end(),
                              [&](const std::shared_ptr<Instruction> &instruction) {
                                  instruction->clear_operands();
                              });
                block->clear_operands();
                block->set_deleted();
                block->get_instructions().clear();
            }
            it = module->get_functions().erase(it);
            get_analysis_result<ControlFlowGraph>(module)->remove(func);
            get_analysis_result<DominanceGraph>(module)->remove(func);
        } else {
            ++it;
        }
    }
}
}
