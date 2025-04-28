#ifndef RV_VARIABLE_TYPE_H
#define RV_VARIABLE_TYPE_H

#include <string>
#include <sstream>
#include "Mir/Structure.h"
#include "Mir/Init.h"
#include "Mir/Type.h"

namespace RISCV::Variables {
    enum VariableType {
        INT32,
        INT64,
        FLOAT,
        DOUBLE,
        STRING,
        VOID,
        BOOL,
        POINTER,
        LABEL,
        ARRAY,
    };

    class Variable {
        public:
            std::string name;
            RISCV::Variables::VariableType type;
            std::string init_value;

            [[nodiscard]] std::string to_string() const;

            [[nodiscard]] virtual std::string to_riscv_indicator() const;

            [[nodiscard]] virtual std::string riscv_signal() const;

            explicit Variable() = default;

            explicit Variable(const std::string &name, const RISCV::Variables::VariableType &type, const std::string &init_value)
                : name(name), type(type), init_value(init_value) {}
    };

    class Array : public Variable {
        public:
            int64_t size;
            RISCV::Variables::VariableType element_type;

            [[nodiscard]] std::string to_riscv_indicator() const override;

            explicit Array() = default;

            explicit Array(const std::string &name, const RISCV::Variables::VariableType &element_type, const std::string &init_value, int64_t size)
                : Variable(name, VariableType::ARRAY, init_value), size(size), element_type(element_type) {}
    };
}

namespace RISCV::Variables::VariableTypeUtils {
    std::string to_string(const VariableType type);

    [[nodiscard]] std::string to_riscv_indicator(const VariableType &type);

    [[nodiscard]] VariableType llvm_to_riscv(const Mir::Type::Type &global_variables);

    [[nodiscard]] int64_t type_to_size(const VariableType &type);
}

namespace RISCV::Variables::VariableInitValueUtils {
    [[nodiscard]] std::string load_from_llvm(const Mir::Init::Init &value, const VariableType &type);

    [[nodiscard]] std::string load_from_llvm(const Mir::Init::Array &value, const VariableType &type, int64_t size);
}

#endif