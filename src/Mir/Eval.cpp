#include <cmath>

#include "Mir/Builder.h"
#include "Mir/Const.h"
#include "Mir/Init.h"
#include "Utils/Log.h"

using namespace Mir;

template<typename Op>
eval_t apply(const eval_t &lhs, const eval_t &rhs, Op op) {
    if (lhs.holds<int>() && rhs.holds<int>()) {
        return op(lhs.get<int>(), rhs.get<int>());
    }
    return op(lhs.get<double>(), rhs.get<double>());
}

eval_t eval_lVal(const std::shared_ptr<AST::LVal> &lVal, const std::shared_ptr<Symbol::Table> &table) {
    const std::string &ident = lVal->ident();
    const auto &symbol = table->lookup_in_all_scopes(ident);
    if (symbol == nullptr) {
        log_fatal("Undefined variable: %s", ident.c_str());
    }
    auto init_value = symbol->get_init_value();
    if (init_value == nullptr) {
        log_error("Undefined variable: %s", ident.c_str());
    }
    std::vector<int> indexes;
    for (const auto &exp: lVal->exps()) {
        const auto &eval_result = eval_exp(exp->addExp(), table);
        int idx = eval_result.get<int>();
        if (!std::holds_alternative<int>(eval_result)) {
            log_warn("Index of non-integer: %f", std::get<double>(eval_result));
        }
        if (idx < 0) { log_error("Index out of bounds: %d", idx); }
        indexes.push_back(idx);
    }
    if (init_value->is_constant_init()) {
        if (!indexes.empty()) {
            log_error("Non-array variable %s", ident.c_str());
        }
        const auto &constant_value = std::static_pointer_cast<Init::Constant>(init_value)->get_const_value();
        if (!constant_value->is_constant()) { log_error("Non-constant expression"); }
        return constant_value->as<Const>()->get_constant_value();
    }
    if (init_value->is_array_init()) {
        init_value = std::static_pointer_cast<Init::Array>(init_value)->get_init_value(indexes);
        if (!init_value->is_constant_init()) { log_error("Non-constant expression"); }
        const auto &constant_value = std::static_pointer_cast<Init::Constant>(init_value)->get_const_value();
        if (!constant_value->is_constant()) { log_error("Non-constant expression"); }
        return constant_value->as<Const>()->get_constant_value();
    }
    log_error("Unknown constant type");
}

eval_t eval(const eval_t lhs, const eval_t rhs, const Token::Type type) {
    switch (type) {
        case Token::Type::ADD: return apply(lhs, rhs, std::plus<>());
        case Token::Type::SUB: return apply(lhs, rhs, std::minus<>());
        case Token::Type::MUL: return apply(lhs, rhs, std::multiplies<>());
        case Token::Type::DIV: {
            return apply(lhs, rhs, [](auto a, auto b) {
                if (b == 0) { log_error("Division by zero"); }
                return a / b;
            });
        }
        case Token::Type::MOD: {
            return apply(lhs, rhs, [](auto a, auto b) -> eval_t {
                if (b == 0) { log_error("Modulo by zero"); }
                if constexpr (std::is_integral_v<decltype(a)> && std::is_integral_v<decltype(b)>) {
                    return a % b;
                } else {
                    return std::fmod(a, b);
                }
            });
        }
        default:
            log_fatal("Unknown operator");
    }
}

eval_t eval_number(const std::shared_ptr<AST::Number> &number) {
    const auto &p = *number;
    if (typeid(p) == typeid(AST::IntNumber)) {
        const auto &int_num = std::static_pointer_cast<AST::IntNumber>(number);
        return int_num->get_value();
    }
    if (typeid(p) == typeid(AST::FloatNumber)) {
        const auto &float_num = std::static_pointer_cast<AST::FloatNumber>(number);
        return float_num->get_value();
    }
    log_fatal("Fatal at eval number");
}

eval_t eval_primaryExp(const std::shared_ptr<AST::PrimaryExp> &primaryExp,
                       const std::shared_ptr<Symbol::Table> &table) {
    if (primaryExp->is_number()) {
        const auto number = std::get<std::shared_ptr<AST::Number>>(primaryExp->get_value());
        return eval_number(number);
    }
    if (primaryExp->is_lVal()) {
        const auto lVal = std::get<std::shared_ptr<AST::LVal>>(primaryExp->get_value());
        return eval_lVal(lVal, table);
    }
    if (primaryExp->is_exp()) {
        const auto exp = std::get<std::shared_ptr<AST::Exp>>(primaryExp->get_value());
        return eval_exp(exp->addExp(), table);
    }
    log_fatal("Fatal at eval primaryExp");
}

eval_t eval_unaryExp(const std::shared_ptr<AST::UnaryExp> &unaryExp, const std::shared_ptr<Symbol::Table> &table) {
    if (unaryExp->is_call()) {
        log_error("Function call cannot be calculated at compile time.");
    }
    if (unaryExp->is_primaryExp()) {
        const auto primaryExp = std::get<std::shared_ptr<AST::PrimaryExp>>(unaryExp->get_value());
        return eval_primaryExp(primaryExp, table);
    }
    if (unaryExp->is_opExp()) {
        using opExp = std::pair<Token::Type, std::shared_ptr<AST::UnaryExp>>;
        const auto [op, unary] = std::get<opExp>(unaryExp->get_value());
        const auto val = eval_unaryExp(unary, table);
        switch (op) {
            case Token::Type::ADD: return val;
            case Token::Type::SUB: {
                if (val.holds<int>()) {
                    return -val.get<int>();
                }
                return -val.get<double>();
            }
            case Token::Type::NOT: {
                const bool is_zero = val.holds<int>() ? val.get<int>() == 0 : val.get<double>() == 0.0f;
                return is_zero ? 1 : 0;
            }
            default: log_fatal("Unknown operator");
        }
    }
    log_fatal("Fatal at eval unaryExp");
}

eval_t eval_mulExp(const std::shared_ptr<AST::MulExp> &mulExp, const std::shared_ptr<Symbol::Table> &table) {
    eval_t res = eval_unaryExp(mulExp->unaryExps()[0], table);
    for (size_t i = 1; i < mulExp->unaryExps().size(); ++i) {
        const eval_t rhs = eval_unaryExp(mulExp->unaryExps()[i], table);
        res = eval(res, rhs, mulExp->operators()[i - 1]);
    }
    return res;
}

eval_t eval_addExp(const std::shared_ptr<AST::AddExp> &addExp, const std::shared_ptr<Symbol::Table> &table) {
    eval_t res = eval_mulExp(addExp->mulExps()[0], table);
    for (size_t i = 1; i < addExp->mulExps().size(); ++i) {
        const eval_t rhs = eval_mulExp(addExp->mulExps()[i], table);
        res = eval(res, rhs, addExp->operators()[i - 1]);
    }
    return res;
}

eval_t eval_exp(const std::shared_ptr<AST::AddExp> &exp, const std::shared_ptr<Symbol::Table> &table) {
    return eval_addExp(exp, table);
}
