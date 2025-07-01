#include "Mir/Structure.h"
#include "Mir/Builder.h"
#include "Mir/Instruction.h"

namespace Mir {
void Module::update_id() const {
    for (const auto &function: functions) { function->update_id(); }
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
            if (!instruction->get_name().empty()) { instruction->set_name(Builder::gen_variable_name()); }
        }
    }
}

void Block::modify_successor(const std::shared_ptr<Block> &old_successor, const std::shared_ptr<Block> &new_successor) const {
    auto terminator = instructions.back();
    if (dynamic_cast<Branch*>(terminator.get()) != nullptr) {        
        terminator->modify_operand(old_successor, new_successor);
    }
    if (dynamic_cast<Jump*>(terminator.get()) != nullptr) {        
        terminator->modify_operand(old_successor, new_successor);
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
