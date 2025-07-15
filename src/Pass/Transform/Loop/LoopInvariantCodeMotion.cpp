#include <unordered_set>
#include "Mir/Builder.h"
#include "Pass/Analysis.h"
#include "Pass/Transforms/Loop.h"

namespace Pass {

void LoopInvariantCodeMotion::transform(std::shared_ptr<Mir::Module> module) {
    module->update_id(); // DEBUG
    const auto cfg_info = get_analysis_result<ControlFlowGraph>(module);
    const auto dom_info = get_analysis_result<DominanceGraph>(module);
    const auto loop_info = get_analysis_result<LoopAnalysis>(module);
    cfg_info->run_on(module);
    module->update_id(); // DEBUG
    loop_info->run_on(module);

    for (auto &func: *module) {
        auto loops = loop_info->loops(func);
        for (auto &loop: loops) {
            if (loop->get_latch() == loop->get_header()) {
            }
        }
    }
}

} // namespace Pass
