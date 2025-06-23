#include <type_traits>

#include "Mir/Instruction.h"
#include "Pass/Util.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
template<typename T, typename... Ts>
inline constexpr bool is_one_of_v = (std::is_same_v<T, Ts> || ...);

template<typename T>
inline constexpr bool is_supported_v = is_one_of_v<T, Add, Mul, FAdd, FMul>;

template<typename BinaryType>
std::shared_ptr<BinaryType> build_balanced(const std::shared_ptr<Block> &block,
                                           const std::shared_ptr<Instruction> &root,
                                           const std::vector<std::shared_ptr<Value>> &leaves,
                                           const size_t lo, const size_t hi) {
    const size_t cnt = hi - lo;
    if (cnt == 1) {
        return leaves[lo]->as<BinaryType>();
    }
    const size_t mid = lo + cnt / 2;
    const auto lhs = build_balanced<BinaryType>(block, root, leaves, lo, mid),
               rhs = build_balanced<BinaryType>(block, root, leaves, mid, hi);
    const auto inst = BinaryType::create("bal", lhs, rhs, block);
    Pass::Utils::move_instruction_before(inst, root);
    return inst;
}

template<typename BinaryType>
void handle(const std::shared_ptr<Block> &block) {
    static_assert(is_supported_v<BinaryType>, "Unsupported binary type");
    using BinaryInst = std::shared_ptr<BinaryType>;
    std::vector<BinaryInst> candidates;
    for (const auto &inst: block->get_instructions()) {
        if (const auto b = inst->is<BinaryType>()) {
            candidates.push_back(b);
        }
    }
    std::unordered_set<std::shared_ptr<Value>> visited;

    for (const auto &root: candidates) {
        if (visited.count(root)) {
            continue;
        }
        std::vector<std::shared_ptr<Value>> leaves;
        std::vector<BinaryInst> cluster;
        auto dfs = [&](auto &&self, const std::shared_ptr<Value> &value) -> void {
            if (auto b = value->is<BinaryType>()) {
                if (visited.insert(b).second) {
                    cluster.push_back(b);
                    self(self, b->get_lhs());
                    self(self, b->get_rhs());
                }
            } else {
                leaves.push_back(value);
            }
        };
        dfs(dfs, root->get_lhs());
        dfs(dfs, root->get_rhs());
        if (leaves.size() < 2) {
            continue;
        }
        const auto new_root = build_balanced<BinaryType>(block, root, leaves, 0, leaves.size());
        root->replace_by_new_value(new_root);
    }
}
}

namespace Pass {
void TreeHeightBalance::run_on_func(const std::shared_ptr<Function> &func) {
    for (const auto &block: func->get_blocks()) {
        handle<Add>(block);
        handle<Mul>(block);
    }
}

void TreeHeightBalance::transform(const std::shared_ptr<Module> module) {
    for (const auto &func: *module) {
        run_on_func(func);
    }
}
}
