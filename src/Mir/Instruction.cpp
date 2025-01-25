#include "Mir/Instruction.h"

namespace Mir {
std::shared_ptr<Type::Type> GetElementPtr::calc_type_(const std::shared_ptr<Value> &addr) {
    const auto type = addr->get_type();
    const auto ptr_type = std::dynamic_pointer_cast<Type::Pointer>(type);
    if (!ptr_type) {
        log_error("First operand must be a pointer type");
    }
    auto target_type = ptr_type->get_contain_type();
    if (const auto array_type = std::dynamic_pointer_cast<Type::Array>(target_type)) {
        return std::make_shared<Type::Pointer>(array_type->get_element_type());
    }
    if (target_type->is_int32() || target_type->is_float()) {
        return std::make_shared<Type::Pointer>(target_type);
    }
    log_fatal("Invalid pointer target type");
}
}
