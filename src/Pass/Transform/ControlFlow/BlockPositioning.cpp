#include "Pass/Analyses/ControlFlowGraph.h"
#include "Pass/Transforms/ControlFlow.h"

using namespace Mir;

namespace {
using BlockPtr = std::shared_ptr<Block>;

// 对控制流图作一次逆后序遍历（以入口为根，优先访问高概率的后继），将遍历顺序直接用作基本块布局
// 大多数边成为 fall-through，开销极低
[[maybe_unused]]
void reverse_postorder_placement(const std::shared_ptr<Function> &func,
                                 const std::shared_ptr<Pass::ControlFlowGraph> &cfg) {
    auto &blocks = func->get_blocks();
    std::vector<BlockPtr> rpo;
    std::unordered_set<BlockPtr> visited;

    auto dfs = [&](auto &&self, const BlockPtr &block) -> void {
        visited.insert(block);
        for (const auto &succ: cfg->graph(func).successors.at(block)) {
            if (!visited.count(succ)) {
                self(self, succ);
            }
        }
        rpo.push_back(block);
    };

    const auto entry = func->get_blocks().front();
    dfs(dfs, entry);
    std::reverse(rpo.begin(), rpo.end());
    blocks = std::move(rpo);
}

// 运行收集 Profile 后重编译，重排序函数内部的基本块
[[maybe_unused]]
void pettis_hansen_placement(const std::shared_ptr<Function> &func,
                             const std::shared_ptr<Pass::ControlFlowGraph> &cfg) {}

// 静态分支概率估计
[[maybe_unused]]
void static_probability_placement(const std::shared_ptr<Function> &func,
                                  const std::shared_ptr<Pass::ControlFlowGraph> &cfg) {}
} // namespace

namespace Pass {
void BlockPositioning::run_on_func(const std::shared_ptr<Function> &func) const {
    reverse_postorder_placement(func, cfg_info);
}

void BlockPositioning::transform(const std::shared_ptr<Module> module) {
    cfg_info = get_analysis_result<ControlFlowGraph>(module);
    for (const auto &func: *module) {
        run_on_func(func);
    }
    cfg_info = nullptr;
}

void BlockPositioning::transform(const std::shared_ptr<Function> &func) {
    cfg_info = get_analysis_result<ControlFlowGraph>(Module::instance());
    run_on_func(func);
    cfg_info = nullptr;
}
} // namespace Pass
