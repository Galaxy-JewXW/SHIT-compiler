#include <algorithm>

#include "Pass/Analysis.h"
#include "Pass/Transform.h"

using namespace Mir;
using FunctionPtr = std::shared_ptr<Function>;
using BlockPtr = std::shared_ptr<Block>;
using InstructionPtr = std::shared_ptr<Instruction>;

static std::shared_ptr<Pass::ControlFlowGraph> cfg = nullptr;
static std::shared_ptr<Pass::LoopAnalysis> loop_analysis = nullptr;

static FunctionPtr current_function = nullptr;
static std::unordered_set<InstructionPtr> visited_instructions{};

// 将指令从其所在的block中移除，并移动到target_block的倒数第二位（在该block的terminator之前）
[[maybe_unused]] static void move_instruction(const InstructionPtr &instruction, const BlockPtr &target_block) {
    const BlockPtr &current_block = instruction->get_block();
    auto &instructions = current_block->get_instructions();
    const auto it = std::find(instructions.begin(), instructions.end(), instruction);
    if (it == instructions.end()) [[unlikely]] {
        log_error("Instruction %s not in block %s", instruction->to_string().c_str(),
                  current_block->get_name().c_str());
    }
    instructions.erase(it);
    instruction->set_block(target_block);
    auto &target_instructions = target_block->get_instructions();
    if (target_instructions.empty()) {
        log_error("Empty block %s", target_block->get_name().c_str());
    }
    target_instructions.insert(target_instructions.end() - 1, instruction);
}

// 有些指令是无法被灵活调度的，它们受到控制依赖的牵制，无法被调度到其他基本块。
// TODO：考虑某些情况下，CALL是可被调度的
[[maybe_unused]] static bool is_pinned(const InstructionPtr &instruction) {
    switch (instruction->get_op()) {
        case Operator::BRANCH:
        case Operator::RET:
        case Operator::PHI:
        case Operator::STORE:
        case Operator::LOAD:
        case Operator::CALL:
            return true;
        default:
            return false;
    }
}

// 尽可能的把指令前移，确定每个指令能被调度到的最早的基本块，同时不影响指令间的依赖关系。
// 当我们把指令向前提时，限制它前移的是它的输入，即每条指令最早要在它的所有输入定义后的位置。
[[maybe_unused]] static void schedule_early(const InstructionPtr &instruction) {
    if (is_pinned(instruction)) {
        return;
    }
    if (visited_instructions.count(instruction)) {
        return;
    }
    visited_instructions.insert(instruction);
    const BlockPtr &entry_block = current_function->get_blocks().front();
    move_instruction(instruction, entry_block);
    // 遍历instruction的操作数
    for (const auto &operand: *instruction) {
        if (const auto &operand_instruction = std::dynamic_pointer_cast<Instruction>(operand)) {
            schedule_early(operand_instruction);
        }
    }
}

// 尽可能的把指令后移，确定每个指令能被调度到的最晚的基本块。
// 每个指令也会被使用它们的指令限制，限制其不能无限向后移。
[[maybe_unused]] static void schedule_late(const InstructionPtr &instruction) {}

[[maybe_unused]] static void run_on_func(const FunctionPtr &func) {
    current_function = func;
    std::vector<BlockPtr> post_order_blocks = cfg->post_order_blocks(func);
    std::reverse(post_order_blocks.begin(), post_order_blocks.end());
    // 预先计算所需的总容量
    size_t total_instructions = 0;
    for (const auto &block: post_order_blocks) {
        total_instructions += block->get_instructions().size();
    }
    // 创建一个临时的指令列表快照，用于存储所有指令
    std::vector<InstructionPtr> snap_instructions;
    snap_instructions.reserve(total_instructions);
    for (const auto &block: post_order_blocks) {
        for (const auto &instruction: block->get_instructions()) {
            snap_instructions.push_back(instruction);
        }
    }
    // for (const auto &instruction: snap_instructions) {
    //     schedule_early(instruction);
    // }
}

namespace Pass {
void GlobalCodeMotion::transform(const std::shared_ptr<Module> module) {
    // 计算支配树和支配关系
    cfg = create<ControlFlowGraph>();
    cfg->run_on(module);
    // 利用循环分析计算循环深度
    loop_analysis = create<LoopAnalysis>();
    loop_analysis->run_on(module);

    visited_instructions.clear();
    current_function = nullptr;
    for (const auto &func: *module) {
        run_on_func(func);
    }
    cfg = nullptr;
    loop_analysis = nullptr;
}
}
