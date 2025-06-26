#ifndef RV_VARIABLE_TYPES_H
#define RV_VARIABLE_TYPES_H

#include <sstream>
#include "Mir/Structure.h"
#include "Mir/Init.h"
#include "Mir/Type.h"

#define __BYTE__ 1

namespace Backend {
    enum VariableType {
        INT1, INT1_PTR,
        INT8, INT8_PTR,
        INT32, INT32_PTR,
        INT64, INT64_PTR,
        FLOAT, FLOAT_PTR,
        DOUBLE, DOUBLE_PTR,
        STRING, STRING_PTR,
        VOID, LABEL,
    };
}

namespace Backend::Utils {
    [[nodiscard]] VariableType llvm_to_riscv(const Mir::Type::Type& type);
    [[nodiscard]] VariableType to_pointer(VariableType type);
    [[nodiscard]] VariableType to_reference(VariableType type);
    [[nodiscard]] size_t type_to_size(VariableType type);
    [[nodiscard]] std::string to_string(VariableType type);
    [[nodiscard]] std::string to_riscv_indicator(VariableType type);
    [[nodiscard]] bool is_pointer(VariableType type);
}

#endif