#include <numeric>
#include <unordered_set>

#include "Mir/Instruction.h"
#include "Pass/Util.h"

namespace Pass::Utils {
using namespace Mir;

std::string format_blocks(const std::unordered_set<std::shared_ptr<Block>> &blocks) {
    if (blocks.empty()) return "∅";
    std::vector<std::string> names;
    names.reserve(blocks.size());
    for (const auto &b: blocks) names.push_back("\'" + b->get_name() + "\'");
    return std::accumulate(
        std::next(names.begin()), names.end(), names[0],
        [](const std::string &a, const std::string &b) { return a + ", " + b; }
    );
}

void move_instruction_before(const std::shared_ptr<Instruction> &instruction,
                             const std::shared_ptr<Instruction> &target) {
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
        const std::shared_ptr<Instruction> &instr_copy = instruction;
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

void delete_instruction_set(const std::shared_ptr<Module> &module,
                            const std::unordered_set<std::shared_ptr<Instruction>> &deleted_instructions) {
    for (const auto &function: *module) {
        for (const auto &block: function->get_blocks()) {
            for (auto it = block->get_instructions().begin(); it != block->get_instructions().end();) {
                if (deleted_instructions.count(*it)) {
                    it = block->get_instructions().erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
}
}
