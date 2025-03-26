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
static void move_instruction(const InstructionPtr &instruction, const BlockPtr &target_block) {
    if (instruction == nullptr || target_block == nullptr) {
        log_fatal("nullptr instruction or block");
    }
    const BlockPtr &current_block = instruction->get_block();
    auto &instructions = current_block->get_instructions();
    const auto it = std::find(instructions.begin(), instructions.end(), instruction);
    if (it == instructions.end()) [[unlikely]] {
        log_error("Instruction %s not in block %s", instruction->to_string().c_str(),
                  current_block->get_name().c_str());
    }
    instructions.erase(it);
    instruction->set_block(target_block, false);
    auto &target_instructions = target_block->get_instructions();
    if (target_instructions.empty()) {
        log_error("Empty block %s", target_block->get_name().c_str());
    }
    target_instructions.insert(target_instructions.end() - 1, instruction);
}

// 将指令从其所在的block中移除，并移动到target之前
static void move_instruction_before(const InstructionPtr &instruction, const InstructionPtr &target) {
    if (instruction == nullptr || target == nullptr) {
        log_fatal("nullptr instruction or target");
    }
    const auto &current_block = instruction->get_block();
    // 如果源块和目标块是同一个块，需要特别处理避免迭代器失效
    const auto &target_block = target->get_block();
    if (current_block == target_block) {
        auto &instructions = current_block->get_instructions();
        // 找到两个指令的位置
        const auto instr_pos = std::distance(
            instructions.begin(),
            std::find(instructions.begin(), instructions.end(), instruction)
        );
        auto target_pos = std::distance(
            instructions.begin(),
            std::find(instructions.begin(), instructions.end(), target)
        );
        if (static_cast<size_t>(instr_pos) == instructions.size()) [[unlikely]] {
            log_error("Instruction %s not in block %s", instruction->to_string().c_str(),
                      current_block->get_name().c_str());
        }
        if (static_cast<size_t>(target_pos) == instructions.size()) [[unlikely]] {
            log_error("Instruction %s not in block %s", target->to_string().c_str(),
                      target_block->get_name().c_str());
        }
        // 如果指令已经在目标之前且相邻，则无需移动
        if (instr_pos + 1 == target_pos) {
            return;
        }
        // 暂时保存指令，先从原位置移除
        const InstructionPtr &instr_copy = instruction;
        instructions.erase(instructions.begin() + instr_pos);
        // 由于移除元素后，如果target_pos > instr_pos，target位置需要调整
        if (target_pos > instr_pos) {
            --target_pos;
        }
        // 插入到target之前
        instructions.insert(instructions.begin() + target_pos, instr_copy);
        return;
    }
    auto &instructions = current_block->get_instructions();
    auto &target_instructions = target_block->get_instructions();
    if (const auto it = std::find(instructions.begin(), instructions.end(), instruction);
        it == instructions.end()) [[unlikely]] {
        log_error("Instruction %s not in block %s", instruction->to_string().c_str(),
                  current_block->get_name().c_str());
    } else {
        instructions.erase(it);
    }
    instruction->set_block(target_block, false);
    if (const auto it = std::find(target_instructions.begin(), target_instructions.end(), target);
        it == target_instructions.end()) [[unlikely]] {
        log_error("Instruction %s not in block %s", target->to_string().c_str(),
                  target_block->get_name().c_str());
    } else {
        target_instructions.insert(it, instruction);
    }
}

// 有些指令是无法被灵活调度的，它们受到控制依赖的牵制，无法被调度到其他基本块。
static bool is_pinned(const InstructionPtr &instruction) {
    switch (instruction->get_op()) {
        case Operator::BRANCH:
        case Operator::JUMP:
        case Operator::RET:
        case Operator::PHI:
        case Operator::STORE:
        case Operator::LOAD:
        case Operator::CALL: // TODO：考虑某些情况下，CALL是可被调度的
            return true;
        default:
            return false;
    }
}

// 计算给定基本块在支配树深度
static int dom_tree_depth(const BlockPtr &block) {
    if (block == nullptr) {
        log_error("BlockPtr cannot be nullptr");
    }
    int depth = 0;
    BlockPtr current = block;
    const auto &imm_dom_map = cfg->immediate_dominator(current_function);
    while (imm_dom_map.find(current) != imm_dom_map.end()) {
        ++depth;
        current = imm_dom_map.at(current);
    }
    return depth;
}

static int loop_depth(const BlockPtr &block) {
    if (block == nullptr) {
        log_error("BlockPtr cannot be nullptr");
    }
    return loop_analysis->get_block_depth(current_function, block);
}

// 计算两个基本块在支配树上的最近公共祖先
static BlockPtr find_lca(const BlockPtr &block1, const BlockPtr &block2) {
    if (block1 == nullptr) {
        return block2;
    }
    if (block2 == nullptr) {
        return block1;
    }
    const auto &imm_dom_map = cfg->immediate_dominator(current_function);
    auto p = block1, q = block2;
    while (dom_tree_depth(p) < dom_tree_depth(q)) {
        q = imm_dom_map.at(q);
    }
    while (dom_tree_depth(p) > dom_tree_depth(q)) {
        p = imm_dom_map.at(p);
    }
    while (p != q) {
        p = imm_dom_map.at(p);
        q = imm_dom_map.at(q);
    }
    return p;
}

// 尽可能的把指令前移，确定每个指令能被调度到的最早的基本块，同时不影响指令间的依赖关系。
// 当我们把指令向前提时，限制它前移的是它的输入，即每条指令最早要在它的所有输入定义后的位置。
static void schedule_early(const InstructionPtr &instruction) {
    if (is_pinned(instruction)) {
        return;
    }
    if (visited_instructions.count(instruction)) {
        return;
    }
    visited_instructions.insert(instruction);
    const BlockPtr &entry_block = current_function->get_blocks().front();
    move_instruction(instruction, entry_block);
    // 遍历instruction的操作数（输入）
    for (const auto &operand: instruction->get_operands()) {
        const auto &input_instruction = std::dynamic_pointer_cast<Instruction>(operand);
        if (input_instruction == nullptr) {
            continue;
        }
        if (dom_tree_depth(instruction->get_block()) < dom_tree_depth(input_instruction->get_block())) {
            move_instruction(instruction, input_instruction->get_block());
        }
    }
}

// 尽可能的把指令后移，确定每个指令能被调度到的最晚的基本块。
// 每个指令也会被使用它们的指令限制，限制其不能无限向后移。
static void schedule_late(const InstructionPtr &instruction) {
    if (is_pinned(instruction)) {
        return;
    }
    if (visited_instructions.count(instruction)) {
        return;
    }
    visited_instructions.insert(instruction);
    BlockPtr lca = nullptr;
    for (const auto &user: instruction->users()) {
        const auto &user_instruction = std::dynamic_pointer_cast<Instruction>(user);
        if (user_instruction == nullptr) {
            continue;
        }
        schedule_late(user_instruction);
        BlockPtr use_block = nullptr;
        if (user_instruction->get_op() == Operator::PHI) {
            const auto &phi = std::static_pointer_cast<Phi>(user_instruction);
            for (const auto &[op_block, op_value]: phi->get_optional_values()) {
                const auto &op_instruction = std::dynamic_pointer_cast<Instruction>(op_value);
                if (op_instruction == nullptr) {
                    continue;
                }
                if (op_instruction == instruction) {
                    use_block = op_block;
                    lca = find_lca(use_block, lca);
                }
            }
        } else {
            use_block = user_instruction->get_block();
            lca = find_lca(use_block, lca);
        }
    }
    if (instruction->users().size() != 0) {
        if (lca == nullptr) {
            log_error("LCA is null for instruction %s", instruction->to_string().c_str());
        }
        BlockPtr select = lca;
        while (lca != instruction->get_block() && lca != current_function->get_blocks().front()) {
            if (lca == nullptr) { log_error("lca cannot be nullptr"); }
            lca = cfg->immediate_dominator(current_function).at(lca);
            if (lca == nullptr) { log_error("lca cannot be nullptr"); }
            if (loop_depth(lca) < loop_depth(select) || [&] {
                const auto &succ = cfg->successors(current_function).at(lca);
                return succ.size() == 1 && succ.find(select) != succ.end();
            }()) {
                select = lca;
            }
        }
        move_instruction(instruction, select);
    }
    const BlockPtr current_block = instruction->get_block();
    for (const auto &inst: current_block->get_instructions()) {
        if (inst != instruction && inst->get_op() != Operator::PHI) {
            for (const auto &operand: inst->get_operands()) {
                if (operand == instruction) {
                    move_instruction_before(instruction, inst);
                    return;
                }
            }
        }
    }
}

static void run_on_func(const FunctionPtr &func) {
    current_function = func;
    visited_instructions.clear();
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
    for (const auto &instruction: snap_instructions) {
        schedule_early(instruction);
    }
    visited_instructions.clear();
    std::reverse(snap_instructions.begin(), snap_instructions.end());
    for (const auto &instruction: snap_instructions) {
        schedule_late(instruction);
    }
}

namespace Pass {
void GlobalCodeMotion::transform(const std::shared_ptr<Module> module) {
    // 计算支配树和支配关系
    cfg = get_analysis_result<ControlFlowGraph>(module);
    // 利用循环分析计算循环深度
    loop_analysis = get_analysis_result<LoopAnalysis>(module);

    visited_instructions.clear();
    current_function = nullptr;
    for (const auto &func: *module) {
        run_on_func(func);
    }
    cfg = nullptr;
    loop_analysis = nullptr;
    current_function = nullptr;
    visited_instructions.clear();
}
}
