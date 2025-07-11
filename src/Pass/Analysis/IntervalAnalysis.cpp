#include "Pass/Analyses/IntervalAnalysis.h"
#include "Pass/Transforms/ControlFlow.h"

using namespace Mir;

namespace {
using _IntervalSetInt = Pass::IntervalAnalysis::IntervalSet<int>;
using _IntervalSetDouble = Pass::IntervalAnalysis::IntervalSet<double>;
using AnyIntervalSet = std::variant<_IntervalSetInt, _IntervalSetDouble>;

struct Summary {
    using ConditionsMap = std::unordered_map<std::shared_ptr<Value>, AnyIntervalSet>;

    // 前置条件
    ConditionsMap constraints{};
    // 后置条件
    ConditionsMap post_conditions{};

    explicit Summary() = default;

    bool operator==(const Summary &other) const {
        return constraints == other.constraints && post_conditions == other.post_conditions;
    }

    bool operator!=(const Summary &other) const { return !(*this == other); }
};

class SummaryManager {
public:
    using FunctionSummaryMap = std::unordered_map<std::shared_ptr<Function>, Summary>;

    void update(const std::shared_ptr<Function> &func, const Summary &s) { summaries_[func] = s; }

    Summary get(const std::shared_ptr<Function> &func) const {
        if (const auto it = summaries_.find(func); it != summaries_.end()) {
            return it->second;
        }
        // 如果找不到摘要，返回一个空的默认摘要
        return Summary{};
    }

private:
    FunctionSummaryMap summaries_;
};
} // namespace

namespace Pass {
void IntervalAnalysis::analyze(const std::shared_ptr<const Module> module) {
    // 保证对于每一个函数，只有一个返回点
    create<SingleReturnTransform>()->run_on(std::const_pointer_cast<Module>(module));
    cfg_info = get_analysis_result<ControlFlowGraph>(module);
    func_info = get_analysis_result<FunctionAnalysis>(module);

    const auto &topo{func_info->topo()};
    std::unordered_set worklist(topo.begin(), topo.end());

    SummaryManager summary_manager;

    while (!worklist.empty()) {
        const auto func{*worklist.begin()};
        worklist.erase(worklist.begin());
        const auto old_summary{summary_manager.get(func)};
        // TODO: analyse function
        Summary new_summary;
        summary_manager.update(func, new_summary);
        if (old_summary != new_summary) {
            for (const auto &g: func_info->call_graph_reverse_func(func)) {
                if (worklist.find(g) == worklist.end()) {
                    worklist.insert(g);
                }
            }
        }
    }

    cfg_info = nullptr;
    func_info = nullptr;
}
} // namespace Pass
