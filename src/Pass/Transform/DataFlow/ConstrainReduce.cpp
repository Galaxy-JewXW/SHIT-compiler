#include "Pass/Analyses/IntervalAnalysis.h"
#include "Pass/Transforms/DataFlow.h"

namespace Pass {
void ConstrainReduce::transform(const std::shared_ptr<Mir::Module> module) {
    const auto interval = get_analysis_result<IntervalAnalysis>(module);
}
}
