#include "Mir/Init.h"
#include "Mir/Builder.h"
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

std::shared_ptr<Store> Constant::gen_store_inst(const std::shared_ptr<Alloc> &addr) {
    return nullptr;
}

std::shared_ptr<Store> Exp::gen_store_inst(const std::shared_ptr<Alloc> &addr) {
    auto self = std::static_pointer_cast<Exp>(shared_from_this());
    const auto exp = BasicInitTrait<Exp>::get_init_value(self);
    return nullptr;
}

std::shared_ptr<Store> Array::gen_store_inst(const std::shared_ptr<Alloc> &addr) {
    return nullptr;
}
}
