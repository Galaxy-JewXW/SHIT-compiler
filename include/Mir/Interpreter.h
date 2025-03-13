#ifndef INTERPRETER_H
#define INTERPRETER_H
#include <memory>
#include <unordered_map>
#include <variant>

#include "Structure.h"

namespace Mir {
class Interpreter {
public:
    struct eval_t : std::variant<int, double> {
        using variant::variant;

        template<typename T>
        explicit operator T() const {
            return std::visit([](auto v) -> T {
                return static_cast<T>(v);
            }, *this);
        }

        explicit eval_t(const size_t t) : variant(static_cast<int>(t)) {}
    };

private:
    eval_t func_return_value = 0;
    std::shared_ptr<Block> current_block = nullptr;
    std::shared_ptr<Block> prev_block = nullptr;
    std::vector<eval_t> eval_stack{};
    std::unordered_map<std::shared_ptr<Value>, eval_t> value_map{};
    std::unordered_map<std::shared_ptr<Value>, eval_t> phi_map{};

public:
    // args是传入的实参
    eval_t interpret_function(const std::shared_ptr<Function> &func,
                              const std::vector<eval_t> &real_args);
};
}

#endif //INTERPRETER_H
