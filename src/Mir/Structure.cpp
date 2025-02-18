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
}
