#include "Mir/Init.h"
#include "Mir/Builder.h"
#include "Utils/Log.h"

namespace Mir::Init {
std::shared_ptr<Constant>
Constant::create_constant_init_value(const std::shared_ptr<Type::Type> &type,
                                     const std::shared_ptr<AST::AddExp> &addExp,
                                     const std::shared_ptr<Symbol::Table> &table) {
    const auto &res = eval_exp(addExp, table);
    if (type->is_int32()) {
        if (std::holds_alternative<int>(res)) {
            return std::make_shared<Constant>(
                type, std::make_shared<ConstInt>(std::get<int>(res)));
        }
        if (std::holds_alternative<float>(res)) {
            return std::make_shared<Constant>(
                type, std::make_shared<ConstInt>(static_cast<int>(std::get<float>(res))));
        }
    }
    if (type->is_float()) {
        if (std::holds_alternative<int>(res)) {
            return std::make_shared<Constant>(
                type, std::make_shared<ConstFloat>(static_cast<float>(std::get<int>(res))));
        }
        if (std::holds_alternative<float>(res)) {
            return std::make_shared<Constant>(
                type, std::make_shared<ConstFloat>(std::get<float>(res)));
        }
    }
    log_error("Illegal type: %s", type->to_string().c_str());
}

std::shared_ptr<Constant>
Constant::create_zero_constant_init_value(const std::shared_ptr<Type::Type> &type) {
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


}
