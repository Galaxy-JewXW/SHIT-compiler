#include <type_traits>

#include "Mir/Instruction.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
template<typename T, typename... Ts>
inline constexpr bool is_one_of_v = (std::is_same_v<T, Ts> || ...);

template<typename BinaryType>
std::vector<std::shared_ptr<BinaryType>> make_chain(const std::shared_ptr<Block> &block) {
    std::vector<std::shared_ptr<BinaryType>> chain;
    const auto &instructions = block->get_instructions();
    for (const auto &inst: instructions) {
        if (const auto binary = inst->is<BinaryType>()) {
            if (chain.empty() || binary->get_lhs() == chain.back()) {
                chain.push_back(binary);
            } else {
                break;
            }
        } else if (!chain.empty()) {
            break;
        }
    }
    return chain;
}

template<typename BinaryType>
std::shared_ptr<BinaryType> build_balanced(const std::shared_ptr<Block> &block,
                                           const std::vector<std::shared_ptr<Value>> &operands,
                                           const std::vector<std::shared_ptr<BinaryType>> &chain,
                                           const size_t start, const size_t end) {
    const size_t count = end - start;
    if (count == 1) {
        return operands[start]->as<BinaryType>();
    }
    const size_t mid = start + count / 2;
    const auto lhs = build_balanced<BinaryType>(block, operands, chain, start, mid),
               rhs = build_balanced<BinaryType>(block, operands, chain, mid, end);
    const auto new_add = Add::create("add", lhs, rhs, nullptr);
    new_add->set_block(block, false);
    auto &instructions = block->get_instructions();
    auto pos = std::find(instructions.begin(), instructions.end(), chain.back());
    instructions.insert(pos, new_add);
    return new_add;
}

template<typename BinaryType>
void handle(const std::shared_ptr<Block> &block) {
    static_assert(is_one_of_v<BinaryType, Add, Mul, FAdd, FMul>,
                  "Only support commutative and associative instructions");
    const auto chain = make_chain<BinaryType>(block);
    if (chain.size() < 2) {
        return;
    }
    std::vector<std::shared_ptr<Value>> operands;
    operands.push_back(chain.front()->get_lhs());
    for (const auto &inst: chain) {
        operands.push_back(inst->get_rhs());
    }
    const auto new_add = build_balanced<BinaryType>(block, operands, chain, 0, operands.size());
    chain.back()->replace_by_new_value(new_add);
}
}

namespace Pass {
void TreeHeightBalance::run_on_func(const std::shared_ptr<Function> &func) {
    for (const auto &block: func->get_blocks()) {
        handle<Add>(block);
    }
}

void TreeHeightBalance::transform(const std::shared_ptr<Module> module) {
    for (const auto &func: *module) {
        run_on_func(func);
    }
}
}
