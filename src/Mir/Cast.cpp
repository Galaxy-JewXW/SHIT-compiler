#include <Mir/Instruction.h>

#include "Mir/Builder.h"
#include "Utils/Log.h"

template<typename From, typename To>
To cast_constant(From value) {
    if constexpr (std::is_same_v<From, bool>) {
        if constexpr (std::is_same_v<To, int>) return value ? 1 : 0;
        if constexpr (std::is_same_v<To, float>) return value ? 1.0f : 0.0f;
    }
    if constexpr (std::is_same_v<From, int>) {
        if constexpr (std::is_same_v<To, bool>) return value != 0;
        if constexpr (std::is_same_v<To, float>) return static_cast<float>(value);
    }
    if constexpr (std::is_same_v<From, float>) {
        if constexpr (std::is_same_v<To, int>) return static_cast<int>(value);
        if constexpr (std::is_same_v<To, bool>) return value != 0.0f;
    }
    log_error("Invalid constant cast");
}

namespace Mir {
std::shared_ptr<Value> cast_constant_value(const std::shared_ptr<Const> &v,
                                           const std::shared_ptr<Type::Type> &target_type) {
    const auto &src_type = v->get_type();
    if (*src_type == *target_type) return v;
    if (src_type->is_int1()) {
        const auto val = std::any_cast<int>(v->get_constant_value());
        if (target_type->is_int32()) return std::make_shared<ConstInt>(cast_constant<bool, int>(val));
        if (target_type->is_float()) return std::make_shared<ConstFloat>(cast_constant<bool, float>(val));
    } else if (src_type->is_int32()) {
        const auto val = std::any_cast<int>(v->get_constant_value());
        if (target_type->is_int1()) return std::make_shared<ConstBool>(cast_constant<int, bool>(val));
        if (target_type->is_float()) return std::make_shared<ConstFloat>(cast_constant<int, float>(val));
    } else if (src_type->is_float()) {
        const auto val = std::any_cast<float>(v->get_constant_value());
        if (target_type->is_int32()) return std::make_shared<ConstInt>(cast_constant<float, int>(val));
        if (target_type->is_int1()) return std::make_shared<ConstBool>(cast_constant<float, bool>(val));
    }
    log_error("Invalid constant cast");
}

std::shared_ptr<Value> cast(const std::shared_ptr<Value> &v, const std::shared_ptr<Type::Type> &target_type,
                            const std::shared_ptr<Block> &block) {
    const auto &src_type = v->get_type();
    auto check_type = [](const std::shared_ptr<Type::Type> &type) {
        return type->is_integer() || type->is_float();
    };
    if (!check_type(src_type) || !check_type(target_type)) { log_error("Invalid cast"); }
    if (const auto c = std::dynamic_pointer_cast<Const>(v)) { return cast_constant_value(c, target_type); }
    if (*src_type == *target_type) return v;

    if (src_type->is_int1()) {
        if (target_type->is_int32())
            return Zext::create(Builder::gen_variable_name(), v, block);
        if (target_type->is_float())
            return Sitofp::create(Builder::gen_variable_name(),
                                  Zext::create(Builder::gen_variable_name(), v, block),
                                  block);
    } else if (src_type->is_int32()) {
        if (target_type->is_int1())
            return Icmp::create(Builder::gen_variable_name(), Icmp::Op::NE, v,
                                std::make_shared<ConstInt>(0), block);
        if (target_type->is_float())
            return Sitofp::create(Builder::gen_variable_name(), v, block);
    } else if (src_type->is_float()) {
        if (target_type->is_int32())
            return Fptosi::create(Builder::gen_variable_name(), v, block);
        if (target_type->is_int1())
            return Icmp::create(Builder::gen_variable_name(), Icmp::Op::NE, v,
                                std::make_shared<ConstFloat>(0.0f), block);
    }
    log_error("Invalid cast");
}
}
