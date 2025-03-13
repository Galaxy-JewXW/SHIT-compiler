#ifndef INTERPRETER_H
#define INTERPRETER_H
#include <memory>
#include <unordered_map>
#include <variant>

#include "Instruction.h"
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

    // args是传入的实参
    eval_t interpret_function(const std::shared_ptr<Function> &func,
                              const std::vector<eval_t> &real_args);

private:
    eval_t func_return_value = 0;
    std::shared_ptr<Block> current_block = nullptr;
    std::shared_ptr<Block> prev_block = nullptr;
    std::vector<eval_t> eval_stack{};
    std::unordered_map<std::shared_ptr<Value>, eval_t> value_map{};

    eval_t get_value(const std::shared_ptr<Value> &value);

    void interpret_instruction(const std::shared_ptr<Instruction> &instruction);

    void interpret_br(const std::shared_ptr<Branch> &branch);

    void interpret_jump(const std::shared_ptr<Jump> &jump);

    void interpret_ret(const std::shared_ptr<Ret> &ret);

    void interpret_intbinary(const std::shared_ptr<IntBinary> &binary);

    void interpret_floatbinary(const std::shared_ptr<FloatBinary> &binary);
};
}

#endif //INTERPRETER_H
