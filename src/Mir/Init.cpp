#include "Mir/Init.h"
#include "Mir/Builder.h"
#include "Mir/Instruction.h"
#include "Utils/Log.h"

namespace Mir::Init {
std::shared_ptr<Constant> Constant::create_constant_init_value(const std::shared_ptr<Type::Type> &type,
                                                               const std::shared_ptr<AST::AddExp> &addExp,
                                                               const std::shared_ptr<Symbol::Table> &table) {
    const auto &res = eval_exp(addExp, table);
    if (type->is_int32()) {
        int value = std::visit([](auto &&arg) { return static_cast<int>(arg); }, res);
        return std::make_shared<Constant>(type, std::make_shared<ConstInt>(value));
    }
    if (type->is_float()) {
        float value = std::visit([](auto &&arg) { return static_cast<float>(arg); }, res);
        return std::make_shared<Constant>(type, std::make_shared<ConstFloat>(value));
    }
    log_error("Illegal type: %s", type->to_string().c_str());
}

std::shared_ptr<Constant> Constant::create_zero_constant_init_value(const std::shared_ptr<Type::Type> &type) {
    if (type->is_int32()) {
        return std::make_shared<Constant>(
            type, std::make_shared<ConstInt>(0));
    }
    if (type->is_float()) {
        return std::make_shared<Constant>(
            type, std::make_shared<ConstFloat>(0.0f));
    }
    log_error("Illegal type: %s", type->to_string().c_str());
}

std::shared_ptr<Init> Exp::create_exp_init_value(const std::shared_ptr<Type::Type> &type,
                                                 const std::shared_ptr<Value> &exp_value) {
    if (const auto &const_ = std::dynamic_pointer_cast<Const>(exp_value)) {
        return std::make_shared<Constant>(type, const_);
    }
    return std::make_shared<Exp>(type, exp_value);
}


void Constant::gen_store_inst(const std::shared_ptr<Value> &addr, const std::shared_ptr<Block> &block) {
    if (!addr->get_type()->is_pointer()) { log_error("Illegal type: %s", addr->get_type()->to_string().c_str()); }
    const auto &self = std::static_pointer_cast<Constant>(shared_from_this());
    const auto &value = self->get_const_value();
    const auto &content_type = std::static_pointer_cast<Type::Pointer>(addr->get_type())->get_contain_type();
    const auto &casted_value = type_cast(value, content_type, block);
    Store::create(addr, casted_value, block);
}

void Exp::gen_store_inst(const std::shared_ptr<Value> &addr, const std::shared_ptr<Block> &block) {
    if (!addr->get_type()->is_pointer()) { log_error("Illegal type: %s", addr->get_type()->to_string().c_str()); }
    const auto &self = std::static_pointer_cast<Exp>(shared_from_this());
    const auto &value = self->get_exp_value();
    const auto &content_type = std::static_pointer_cast<Type::Pointer>(addr->get_type())->get_contain_type();
    const auto &casted_value = type_cast(value, content_type, block);
    Store::create(addr, casted_value, block);
}

std::shared_ptr<Array> Array::create_zero_array_init_value(const std::shared_ptr<Type::Type> &type) {
    if (!type->is_array()) {
        log_error("%s is not an array type", type->to_string().c_str());
    }
    const auto array_type = std::static_pointer_cast<Type::Array>(type);
    const size_t total_elements = array_type->get_flattened_size();
    const auto atomic_type = array_type->get_atomic_type();
    std::vector<std::shared_ptr<Init>> flattened;
    flattened.reserve(total_elements);
    for (size_t i = 0; i < total_elements; ++i) {
        flattened.emplace_back(Constant::create_zero_constant_init_value(atomic_type));
    }
    return fold_array(type, flattened);
}


void Array::gen_store_inst(const std::shared_ptr<Value> &addr, const std::shared_ptr<Block> &block,
                           const std::vector<int> &dimensions) {
    if (!addr->get_type()->is_pointer()) { log_error("Illegal type: %s", addr->get_type()->to_string().c_str()); }
    const auto &self = std::static_pointer_cast<Array>(shared_from_this());
    const auto &flattened_init_values = self->get_flattened_init_values();
    std::vector<int> strides(dimensions.size());
    int stride = 1;
    for (auto i = dimensions.size(); i-- > 0;) {
        strides[i] = stride;
        stride *= dimensions[i];
    }
    for (size_t i = 0u; i < flattened_init_values.size(); ++i) {
        auto remaining = i;
        std::vector<int> indexes;
        for (auto j = 0u; j < dimensions.size(); ++j) {
            indexes.push_back(static_cast<int>(remaining) / strides[j]);
            remaining %= strides[j];
        }
        std::shared_ptr<Value> address = addr;
        for (const auto &idx: indexes) {
            address = GetElementPtr::create(Builder::gen_variable_name(), address,
                                            std::make_shared<ConstInt>(idx), block);
        }
        if (const auto &init_value = flattened_init_values[i]; init_value->is_exp_init()) {
            std::static_pointer_cast<Exp>(init_value)->gen_store_inst(address, block);
        } else if (init_value->is_constant_init()) {
            std::static_pointer_cast<Constant>(init_value)->gen_store_inst(address, block);
        } else {
            log_error("Unexpected element of Array initial values");
        }
    }
}
}
