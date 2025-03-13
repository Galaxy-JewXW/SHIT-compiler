#include "Mir/Interpreter.h"
#include "Utils/Log.h"

namespace Mir {
Interpreter::eval_t Interpreter::interpret_function(const std::shared_ptr<Function> &func,
                                                    const std::vector<eval_t> &real_args) {
    const auto &arguments = func->get_arguments();
    if (arguments.size() != real_args.size()) {
        log_error("Wrong number of arguments");
    }
    for (size_t i = 0; i < arguments.size(); i++) {
        value_map[arguments[i]] = real_args[i];
    }
    current_block = func->get_blocks().front();
    while (current_block != nullptr) {
        // for (const auto &instruction : current_block->get_instructions()) {
        //
        // }
    }
    return func_return_value;
}
}
