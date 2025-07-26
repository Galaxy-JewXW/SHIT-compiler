#include "Backend/VariableTypes.h"

[[nodiscard]] Backend::VariableType Backend::Utils::llvm_to_riscv(const Mir::Type::Type& type) {
    if (type.is_int32()) return Backend::VariableType::INT32;
    if (type.is_int1()) return Backend::VariableType::INT1;
    if (type.is_array()) return Backend::Utils::to_pointer(Backend::Utils::llvm_to_riscv(*dynamic_cast<const Mir::Type::Array *>(&type)->get_element_type()));
    if (type.is_float()) return Backend::VariableType::FLOAT;
    if (type.is_void()) return Backend::VariableType::VOID;
    if (type.is_label()) return Backend::VariableType::LABEL;
    if (type.is_pointer()) return Backend::Utils::to_pointer(Backend::Utils::llvm_to_riscv(*dynamic_cast<const Mir::Type::Pointer *>(&type)->get_contain_type()));
    log_error("Type cannot be converted!");
}

[[nodiscard]] Backend::VariableType Backend::Utils::to_pointer(Backend::VariableType type) {
    switch (type) {
        case Backend::VariableType::INT1: return Backend::VariableType::INT1_PTR;
        case Backend::VariableType::INT8: return Backend::VariableType::INT8_PTR;
        case Backend::VariableType::INT32: return Backend::VariableType::INT32_PTR;
        case Backend::VariableType::INT64: return Backend::VariableType::INT64_PTR;
        case Backend::VariableType::FLOAT: return Backend::VariableType::FLOAT_PTR;
        case Backend::VariableType::DOUBLE: return Backend::VariableType::DOUBLE_PTR;
        case Backend::VariableType::STRING: return Backend::VariableType::STRING_PTR;
        default: return type;
    }
}

[[nodiscard]] Backend::VariableType Backend::Utils::to_reference(Backend::VariableType type) {
    switch (type) {
        case Backend::VariableType::INT1_PTR: return Backend::VariableType::INT1;
        case Backend::VariableType::INT8_PTR: return Backend::VariableType::INT8;
        case Backend::VariableType::INT32_PTR: return Backend::VariableType::INT32;
        case Backend::VariableType::INT64_PTR: return Backend::VariableType::INT64;
        case Backend::VariableType::FLOAT_PTR: return Backend::VariableType::FLOAT;
        case Backend::VariableType::DOUBLE_PTR: return Backend::VariableType::DOUBLE;
        case Backend::VariableType::STRING_PTR: return Backend::VariableType::STRING;
        default: return type;
    }
}

[[nodiscard]] size_t Backend::Utils::type_to_size(const Backend::VariableType &type) {
    switch (type) {
        case INT1: return __BYTE__;
        case INT8: return __BYTE__;
        case INT32: return 4 * __BYTE__;
        case FLOAT: return 4 * __BYTE__;
        default: return 8 * __BYTE__;
    }
}

[[nodiscard]] std::string Backend::Utils::to_riscv_indicator(const Backend::VariableType &type) {
    switch (type) {
        case INT1: return ".byte";
        case INT8: return ".byte";
        case INT32: return ".word";
        case FLOAT: return ".float";
        case DOUBLE: return ".double";
        default: log_error("Type cannot be converted!");
    }
}

[[nodiscard]] bool Backend::Utils::is_pointer(const Backend::VariableType &type) {
    switch (type) {
        case Backend::VariableType::INT1_PTR:
        case Backend::VariableType::INT8_PTR:
        case Backend::VariableType::INT32_PTR:
        case Backend::VariableType::INT64_PTR:
        case Backend::VariableType::FLOAT_PTR:
        case Backend::VariableType::DOUBLE_PTR:
        case Backend::VariableType::STRING_PTR:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] bool Backend::Utils::is_int(const Backend::VariableType &type) {
    switch (type) {
        case Backend::VariableType::INT1:
        case Backend::VariableType::INT8:
        case Backend::VariableType::INT32:
        case Backend::VariableType::INT64:
        case Backend::VariableType::INT1_PTR:
        case Backend::VariableType::INT8_PTR:
        case Backend::VariableType::INT32_PTR:
        case Backend::VariableType::INT64_PTR:
        case Backend::VariableType::STRING_PTR:
        case Backend::VariableType::FLOAT_PTR:
        case Backend::VariableType::DOUBLE_PTR:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] bool Backend::Utils::is_float(const Backend::VariableType &type) {
    switch (type) {
        case Backend::VariableType::FLOAT:
        case Backend::VariableType::DOUBLE:
            return true;
        default:
            return false;
    }
}