#ifndef INIT_H
#define INIT_H

#include <string>

#include "Builder.h"
#include "Const.h"
#include "Type.h"
#include "Utils/AST.h"
#include "Utils/Log.h"

namespace Mir {
class Builder;
}

namespace Mir {
class Block;
class Alloc;
class Store;
class GetElementPtr;
} // namespace Mir

namespace Mir::Symbol {
class Table;
}

template<typename TVal>
struct InitValTrait {};

// 针对 ConstInitVal 的偏特化
template<>
struct InitValTrait<AST::ConstInitVal> {
    using ExpType = AST::ConstExp;

    static bool is_array_vals(const std::shared_ptr<AST::ConstInitVal> &node) { return node->is_constInitVals(); }

    static std::vector<std::shared_ptr<AST::ConstInitVal>>
    get_array_vals(const std::shared_ptr<AST::ConstInitVal> &node) {
        return std::get<std::vector<std::shared_ptr<AST::ConstInitVal>>>(node->get_value());
    }

    static bool is_exp(const std::shared_ptr<AST::ConstInitVal> &node) { return node->is_constExp(); }

    static std::shared_ptr<AST::AddExp> get_addExp(const std::shared_ptr<AST::ConstInitVal> &node) {
        return std::get<std::shared_ptr<AST::ConstExp>>(node->get_value())->addExp();
    }
};

// 针对 InitVal 的偏特化
template<>
struct InitValTrait<AST::InitVal> {
    using ExpType = AST::Exp;

    static bool is_array_vals(const std::shared_ptr<AST::InitVal> &node) { return node->is_initVals(); }

    static std::vector<std::shared_ptr<AST::InitVal>> get_array_vals(const std::shared_ptr<AST::InitVal> &node) {
        return std::get<std::vector<std::shared_ptr<AST::InitVal>>>(node->get_value());
    }

    static bool is_exp(const std::shared_ptr<AST::InitVal> &node) { return node->is_exp(); }

    static std::shared_ptr<AST::AddExp> get_addExp(const std::shared_ptr<AST::InitVal> &node) {
        return std::get<std::shared_ptr<AST::Exp>>(node->get_value())->addExp();
    }
};

namespace Mir::Init {
class Init;
class Array;

class Init : public std::enable_shared_from_this<Init> {
protected:
    std::shared_ptr<Type::Type> type;

public:
    explicit Init(const std::shared_ptr<Type::Type> &type) :
        type{type} {}

    virtual ~Init() = default;

    [[nodiscard]] virtual bool is_constant_init() const { return false; }
    [[nodiscard]] virtual bool is_exp_init() const { return false; }
    [[nodiscard]] virtual bool is_array_init() const { return false; }

    [[nodiscard]] virtual std::string to_string() const = 0;

    template<typename T>
    std::shared_ptr<T> as() {
        return std::static_pointer_cast<T>(shared_from_this());
    }

    [[nodiscard]] std::shared_ptr<Type::Type> get_type() const { return type; }
};

class Constant final : public Init {
    std::shared_ptr<Const> const_value;

public:
    explicit Constant(const std::shared_ptr<Type::Type> &type, const std::shared_ptr<Const> &const_value) :
        Init{type}, const_value{const_value} {}

    [[nodiscard]] bool is_constant_init() const override { return true; }
    [[nodiscard]] std::shared_ptr<Const> get_const_value() const { return const_value; }
    [[nodiscard]] bool is_zero() const { return const_value->is_zero(); }

    void gen_store_inst(const std::shared_ptr<Value> &addr, const std::shared_ptr<Block> &block);

    [[nodiscard]] std::string to_string() const override;

    static std::shared_ptr<Constant> create_constant_init_value(const std::shared_ptr<Type::Type> &type,
                                                                const std::shared_ptr<AST::AddExp> &addExp,
                                                                const std::shared_ptr<Symbol::Table> &table);

    static std::shared_ptr<Constant> create_zero_constant_init_value(const std::shared_ptr<Type::Type> &type);
};

class Exp final : public Init {
    std::shared_ptr<Value> exp_value;

public:
    explicit Exp(const std::shared_ptr<Type::Type> &type, const std::shared_ptr<Value> &exp_value) :
        Init{type}, exp_value{exp_value} {}

    [[nodiscard]] bool is_exp_init() const override { return true; }

    [[nodiscard]] std::shared_ptr<Value> get_exp_value() const { return exp_value; }

    void gen_store_inst(const std::shared_ptr<Value> &addr, const std::shared_ptr<Block> &block);

    [[nodiscard]] std::string to_string() const override { log_error("ExpInit cannot be output as a string"); }

    static std::shared_ptr<Init> create_exp_init_value(const std::shared_ptr<Type::Type> &type,
                                                       const std::shared_ptr<Value> &exp_value);
};

class Array final : public Init {
    const bool is_zero_initialized;
    const std::vector<std::shared_ptr<Init>> init_values;
    int last_non_zero_{-1};

public:
    // 计算最后一个非零元素的索引
    void calculate_last_non_zero() {
        last_non_zero_ = -1;
        if (std::any_of(init_values.begin(), init_values.end(), [](const auto &init) {
            return !init->is_constant_init();
        })) {
            return;
        }
        for (int i = static_cast<int>(init_values.size()) - 1; i >= 0; --i) {
            if (const auto &init = init_values[i]; init->is_constant_init()) {
                if (const auto constant_init = std::static_pointer_cast<Constant>(init); !constant_init->is_zero()) {
                    last_non_zero_ = i;
                    break;
                }
            } else {
                // 如果有非Constant类型的元素，则last_non_zero_无意义
                last_non_zero_ = -1;
                return;
            }
        }
    }

    explicit Array(const std::shared_ptr<Type::Type> &type, const std::vector<std::shared_ptr<Init>> &init_values,
                   const bool is_zero_initialized = false) :
        Init{type}, is_zero_initialized{is_zero_initialized}, init_values{init_values} {
        calculate_last_non_zero();
    }

    [[nodiscard]] const auto &get_init_values() const { return init_values; }

    [[nodiscard]] bool is_array_init() const override { return true; }

    [[nodiscard]] bool zero_initialized() const { return is_zero_initialized; }

    [[nodiscard]] size_t get_size() const { return init_values.size(); }

    [[nodiscard]] int last_non_zero() const { return last_non_zero_; }

    std::shared_ptr<Init> get_init_value(const std::vector<int> &indexes);

    static std::shared_ptr<Array> create_zero_array_init_value(const std::shared_ptr<Type::Type> &type);

    template<typename TVal>
    static std::shared_ptr<Array> create_array_init_value(const std::shared_ptr<Type::Type> &type,
                                                          const std::shared_ptr<TVal> &initVal,
                                                          const std::shared_ptr<Symbol::Table> &table, bool is_constant,
                                                          const Builder *builder = nullptr);

    void gen_store_inst(const std::shared_ptr<Value> &addr, const std::shared_ptr<Block> &block,
                        const std::vector<int> &dimensions) const;

    [[nodiscard]] std::string to_string() const override;
};

template<typename TVal>
bool is_zero_array(const std::shared_ptr<Type::Type> &type, const std::shared_ptr<TVal> &initVal,
                   const std::shared_ptr<Symbol::Table> &table, bool is_constant, const Builder *const builder) {
    using Trait = InitValTrait<TVal>;
    if (!type->is_array()) {
        return false;
    }
    if (!Trait::is_array_vals(initVal)) {
        return false;
    }
    if (Trait::get_array_vals(initVal).empty()) {
        return true;
    }
    const auto &array_type = std::static_pointer_cast<Type::Array>(type);
    const auto element_type = array_type->get_element_type(), atomic_type = array_type->get_atomic_type();
    for (const auto &val: Trait::get_array_vals(initVal)) {
        if (Trait::is_exp(val)) {
            if (is_constant) {
                auto res = eval_exp(Trait::get_addExp(val), table);
                if (atomic_type->is_int32()) {
                    if (const int value = std::visit([](auto &&arg) { return static_cast<int>(arg); }, res); value != 0)
                        return false;
                }
                if (atomic_type->is_float()) {
                    const double value = std::visit([](auto &&arg) { return static_cast<double>(arg); }, res);
                    if (constexpr double tolerance = 1e-6f; std::abs(value - 0.0f) >= tolerance) {
                        return false;
                    }
                }
            } else {
                const auto &exp_value = builder->visit_addExp(Trait::get_addExp(val));
                if (!exp_value->is_constant()) {
                    return false;
                }
                const auto const_exp_value = std::dynamic_pointer_cast<Const>(exp_value);
                if (const_exp_value == nullptr || !const_exp_value->is_zero()) {
                    return false;
                }
            }
        } else if (Trait::is_array_vals(val)) {
            if (!is_zero_array<TVal>(element_type, val, table, is_constant, builder)) {
                return false;
            }
        }
    }
    return true;
}

template<typename TVal>
std::shared_ptr<Array> Array::create_array_init_value(const std::shared_ptr<Type::Type> &type,
                                                      const std::shared_ptr<TVal> &initVal,
                                                      const std::shared_ptr<Symbol::Table> &table,
                                                      const bool is_constant, const Builder *const builder) {
    using Trait = InitValTrait<TVal>;
    if (!type->is_array()) {
        log_error("%s is not an array type", type->to_string().c_str());
    }
    if (!Trait::is_array_vals(initVal)) {
        log_error("Not an array");
    }
    if (is_zero_array<TVal>(type, initVal, table, is_constant, builder)) {
        return create_zero_array_init_value(type);
    }
    const auto &array_type = std::static_pointer_cast<Type::Array>(type);
    const auto &element_type = array_type->get_element_type();
    std::vector<std::shared_ptr<Init>> init_values;
    const auto &vals = Trait::get_array_vals(initVal);
    for (size_t i = 0; i < vals.size(); ++i) {
        const auto &val = vals[i];
        if (init_values.size() >= array_type->get_size())
            break;
        if (Trait::is_array_vals(val)) {
            if (!element_type->is_array()) {
                log_error("Element not an array");
            }
            init_values.emplace_back(
                    Array::create_array_init_value<TVal>(element_type, val, table, is_constant, builder));
        } else if (Trait::is_exp(val)) {
            if (element_type->is_array()) {
                auto basic_type = array_type->get_atomic_type();
                const auto element_array_type = std::static_pointer_cast<Type::Array>(element_type);
                const size_t flatten_size = element_array_type->get_flattened_size();
                std::vector<std::shared_ptr<TVal>> sub_vals;
                size_t cnt = 0;
                for (size_t j = 0; j < flatten_size;) {
                    if (i + cnt >= vals.size())
                        break;
                    sub_vals.emplace_back(vals[i + cnt]);
                    if (!Trait::is_array_vals(vals[i + cnt])) {
                        ++j;
                    } else {
                        j += element_array_type->get_flattened_size();
                    }
                    ++cnt;
                }
                TVal ast_val{sub_vals};
                std::shared_ptr<TVal> wrapped_val = std::make_shared<TVal>(ast_val);
                init_values.emplace_back(
                        Array::create_array_init_value<TVal>(element_type, wrapped_val, table, is_constant, builder));
                i += cnt - 1;
            } else {
                if (is_constant) {
                    init_values.emplace_back(
                            Constant::create_constant_init_value(element_type, Trait::get_addExp(val), table));
                } else {
                    const auto &exp_value = builder->visit_addExp(Trait::get_addExp(val));
                    init_values.emplace_back(Exp::create_exp_init_value(element_type, exp_value));
                }
            }
        }
    }
    while (init_values.size() < array_type->get_size()) {
        if (element_type->is_array()) {
            init_values.emplace_back(create_zero_array_init_value(element_type));
        } else {
            init_values.emplace_back(Constant::create_zero_constant_init_value(element_type));
        }
    }
    return std::make_shared<Array>(type, init_values);
}
} // namespace Mir::Init

#endif
