#include "Mir/Structure.h"
#include "Mir/Builder.h"
#include "Mir/Instruction.h"
#include "Pass/Analyses/LoopAnalysis.h"

namespace Mir {
void Module::update_id() const {
    for (const auto &function: functions) {
        function->update_id();
    }
}

void Function::update_id() const {
    Builder::reset_count();
    for (size_t i = 0; i < arguments.size(); ++i) {
        arguments[i]->set_index(static_cast<int>(i));
        arguments[i]->set_name(Builder::gen_variable_name());
    }
    for (const auto &block: blocks) {
        block->set_name(Builder::gen_block_name());
        for (const auto &instruction: block->get_instructions()) {
            if (!instruction->get_name().empty()) {
                instruction->set_name(Builder::gen_variable_name());
            }
        }
    }
}

void Block::modify_successor(const std::shared_ptr<Block> &old_successor,
                             const std::shared_ptr<Block> &new_successor) const {
    const auto terminator{instructions.back()};
    if (terminator->is<Terminator>() == nullptr) [[unlikely]] {
        log_error("Last instruction should be a terminator: %s", terminator->to_string().c_str());
    }
    switch (terminator->get_op()) {
        case Operator::BRANCH:
        case Operator::JUMP:
        case Operator::SWITCH:
            terminator->modify_operand(old_successor, new_successor);
        case Operator::RET:
            return;
        default:
            log_error("Invalid type");
    }
}

std::shared_ptr<std::vector<std::shared_ptr<Instruction>>> Block::get_phis() const {
    auto phis = std::make_shared<std::vector<std::shared_ptr<Instruction>>>();
    for (auto &instruction: instructions) {
        if (instruction->get_op() == Operator::PHI) {
            auto phi = std::static_pointer_cast<Phi>(instruction);
            phis->push_back(phi);
        }
    }
    return phis;
}

std::shared_ptr<Block> Block::cloneinfo_to_func(const std::shared_ptr<Pass::LoopNodeClone> &clone_info,
                                                const std::shared_ptr<Function> &function) {
    auto block = Block::create("clone", function);
    clone_info->add_value_reflect(shared_from_this(), block);
    for (const auto &instr: this->get_instructions()) {
        instr->cloneinfo_to_block(clone_info, block);
    }
    return block;
}

void Block::fix_clone_info(const std::shared_ptr<Pass::LoopNodeClone> &clone_info) {
    for (const auto &instr: this->get_instructions()) {
        instr->fix_clone_info(clone_info);
    }
}

} // namespace Mir
