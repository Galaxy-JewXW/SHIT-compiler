#ifndef RV_INSTRUCTION_H
#define RV_INSTRUCTION_H

#include <string>
#include <sstream>
#include <vector>
#include <memory>

namespace RISCV_Instructions {
    class Instruction {
        public:
            [[nodiscard]] virtual std::string to_string() const;
    };

    class FunctionField {
        public:
            std::string function_name;
            std::vector<std::shared_ptr<Instruction>> instructions;

            [[nodiscard]] std::string to_string() const;
    };

    class TextField {
        private:
            std::vector<FunctionField> functions;
        public:
            [[nodiscard]] std::string to_string() const;

            void add_function(const FunctionField &function);
    };

    class DataField {
        public:
            std::vector<std::string> const_strings;

            [[nodiscard]] std::string to_string() const;
    };
}

#endif