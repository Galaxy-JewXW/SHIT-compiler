#include "Mir/Structure.h"
#include "Mir/Builder.h"
#include "Mir/Instruction.h"

namespace Mir {
void Module::update_id() {
    for (const auto &function: functions) { function->update_id(); }
}

void Function::update_id() {
    Builder::reset_count();
    for (const auto &arg: arguments) {
        arg->set_name(Builder::gen_variable_name());
    }
    for (const auto &block: blocks) {
        block->set_name(Builder::gen_block_name());
        for (const auto &instruction: block->get_instructions()) {
            if (!instruction->get_name().empty()) { instruction->set_name(Builder::gen_variable_name()); }
        }
    }
}

void Block::modify_successor(const std::shared_ptr<Block> &old_successor, const std::shared_ptr<Block> &new_successor) {
    for (auto &instruction: instructions) {
        if (dynamic_cast<Branch*>(instruction.get()) != nullptr) {
            auto branch = std::static_pointer_cast<Branch>(instruction);
            branch->modify_operand(old_successor, new_successor);
        }
        if (dynamic_cast<Jump*>(instruction.get()) != nullptr) {
            auto jump = std::static_pointer_cast<Jump>(instruction);
            jump->modify_operand(old_successor, new_successor);
        }
    }
}

std::shared_ptr<std::vector<std::shared_ptr<Instruction>>> Block::get_phis() {
    auto phis = std::make_shared<std::vector<std::shared_ptr<Instruction>>>();
    for (auto &instruction: instructions) {
        if (instruction->get_op() == Mir::Operator::PHI) {
            auto phi = std::static_pointer_cast<Phi>(instruction);
            phis->push_back(phi);
        }
    }
    return phis;
}

}
