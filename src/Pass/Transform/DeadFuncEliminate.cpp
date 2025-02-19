#include "Pass/Analysis.h"
#include "Pass/Transform.h"

using namespace Mir;

static std::shared_ptr<Pass::FunctionAnalysis> func_graph = nullptr;

using FunctionPtr = std::shared_ptr<Function>;
using FunctionMap = std::unordered_map<FunctionPtr, std::unordered_set<FunctionPtr>>;
using FunctionSet = std::unordered_set<FunctionPtr>;

namespace Pass {
static void dfs(const FunctionPtr &cur_function, FunctionSet &reachable, const FunctionMap &call_graph) {
    if (reachable.find(cur_function) != reachable.end()) {
        return;
    }
    reachable.insert(cur_function);
    if (call_graph.find(cur_function) != call_graph.end()) {
        for (const auto &func: call_graph.at(cur_function)) {
            dfs(func, reachable, call_graph);
        }
    }
}

void DeadFuncEliminate::transform(const std::shared_ptr<Module> module) {
    func_graph = create<FunctionAnalysis>();
    func_graph->run_on(module);
    const auto main_func = module->get_main_function();
    FunctionSet reachable_functions{};
    dfs(main_func, reachable_functions, func_graph->call_graph());
    for (auto it = module->all_functions().begin(); it != module->all_functions().end();) {
        if (reachable_functions.find(*it) == reachable_functions.end()) {
            const auto func = *it;
            for (const auto& block: func->get_blocks()) {
                std::for_each(block->get_instructions().begin(), block->get_instructions().end(),
                          [&](const std::shared_ptr<Instruction> &instruction) {
                              instruction->clear_operands();
                          });
                block->clear_operands();
                block->set_deleted();
            }
            it = module->all_functions().erase(it);
        } else {
            ++it;
        }
    }
    func_graph = nullptr;
}
}
