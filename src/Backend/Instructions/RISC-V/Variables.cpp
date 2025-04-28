#include "Backend/Instructions/RISC-V/Variables.h"
#include <sstream>
#include "Mir/Structure.h"
#include "Mir/Init.h"
#include "Utils/Log.h"

[[nodiscard]] std::string RISCV::Variables::VariableTypeUtils::to_riscv_indicator(const VariableType &type) {
    switch (type) {
        case INT32: return ".dword";
        case INT64: return ".dword";
        case FLOAT: return ".float";
        case DOUBLE: return ".double";
        case BOOL: return ".byte";
        default: log_error("Type cannot be converted!");
    }
}

[[nodiscard]] int64_t RISCV::Variables::VariableTypeUtils::type_to_size(const VariableType &type) {
    // switch (type) {
    //     case INT32: return 4;
    //     case INT64: return 8;
    //     case FLOAT: return 4;
    //     case DOUBLE: return 8;
    //     case BOOL: return 1;
    //     default: return 4;
    // }
    return 8;
}

[[nodiscard]] RISCV::Variables::VariableType RISCV::Variables::VariableTypeUtils::llvm_to_riscv(const Mir::Type::Type &type) {
    if (type.is_int32()) return RISCV::Variables::VariableType::INT32;
    // if (type.is_int64()) return RISCV::Variables::VariableType::INT64;
    if (type.is_array()) return RISCV::Variables::VariableType::ARRAY;
    if (type.is_int1()) return RISCV::Variables::VariableType::BOOL;
    if (type.is_float()) return RISCV::Variables::VariableType::FLOAT;
    if (type.is_void()) return RISCV::Variables::VariableType::VOID;
    if (type.is_pointer()) return RISCV::Variables::VariableType::POINTER;
    if (type.is_label()) return RISCV::Variables::VariableType::LABEL;
    log_error("Type cannot be converted!");
}

[[nodiscard]] std::string RISCV::Variables::VariableInitValueUtils::load_from_llvm(const Mir::Init::Init &value, const VariableType &type) {
    switch (type) {
        case INT32:
        case INT64: {
            const Mir::Init::Constant *const_value = dynamic_cast<const Mir::Init::Constant *>(&value);
            return const_value->get_const_value()->to_string();
        }
        case DOUBLE:
        case FLOAT: {
            const Mir::Init::Constant *const_value = dynamic_cast<const Mir::Init::Constant *>(&value);
            return const_value->get_const_value()->to_string();
        }
        default: log_error("Init value cannot be converted!");
    }
}

[[nodiscard]] std::string RISCV::Variables::VariableInitValueUtils::load_from_llvm(const Mir::Init::Array &array, const VariableType &type, int64_t size) {
    std::ostringstream oss;
    if (array.zero_initialized()) {
        oss << "0" << "\n    .zero " << (size - 1) * RISCV::Variables::VariableTypeUtils::type_to_size(type);
    }
    return oss.str();
}

[[nodiscard]] std::string RISCV::Variables::Variable::riscv_signal() const {
    return ".global_var_" + name.substr(1, name.size() - 1);
}

[[nodiscard]] std::string RISCV::Variables::Variable::to_string() const {
    std::ostringstream oss;
    oss << this->riscv_signal() << ": " << this->to_riscv_indicator() << " " << this->init_value;
    return oss.str();
}

[[nodiscard]] std::string RISCV::Variables::Variable::to_riscv_indicator() const {
    return RISCV::Variables::VariableTypeUtils::to_riscv_indicator(this->type);
}

[[nodiscard]] std::string RISCV::Variables::Array::to_riscv_indicator() const {
    return RISCV::Variables::VariableTypeUtils::to_riscv_indicator(this->element_type);
}