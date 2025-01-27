#ifndef INIT_H
#define INIT_H

#include <string>

#include "Const.h"
#include "Type.h"
#include "Utils/AST.h"
#include "Utils/Log.h"

namespace Mir {
class Builder;
}

#include "Builder.h"

namespace Mir {
class Block;
class Alloc;
class Store;
class GetElementPtr;
}

template<typename TVal>
struct InitValTrait {};

// 针对 ConstInitVal 的偏特化
template<>
struct InitValTrait<AST::ConstInitVal> {
    using ExpType = AST::ConstExp;

    static bool is_array_vals(const std::shared_ptr<AST::ConstInitVal> &node) {
        return node->is_constInitVals();
    }

    static std::vector<std::shared_ptr<AST::ConstInitVal>>
    get_array_vals(const std::shared_ptr<AST::ConstInitVal> &node) {
        return std::get<std::vector<std::shared_ptr<AST::ConstInitVal>>>(node->get_value());
    }

    static bool is_exp(const std::shared_ptr<AST::ConstInitVal> &node) {
        return node->is_constExp();
    }

    static std::shared_ptr<AST::AddExp> get_addExp(const std::shared_ptr<AST::ConstInitVal> &node) {
        return std::get<std::shared_ptr<AST::ConstExp>>(node->get_value())->addExp();
    }
};

// 针对 InitVal 的偏特化
template<>
struct InitValTrait<AST::InitVal> {
    using ExpType = AST::Exp;

    static bool is_array_vals(const std::shared_ptr<AST::InitVal> &node) {
        return node->is_initVals();
    }

    static std::vector<std::shared_ptr<AST::InitVal>> get_array_vals(const std::shared_ptr<AST::InitVal> &node) {
        return std::get<std::vector<std::shared_ptr<AST::InitVal>>>(node->get_value());
    }

    static bool is_exp(const std::shared_ptr<AST::InitVal> &node) {
        return node->is_exp();
    }

    static std::shared_ptr<AST::AddExp> get_addExp(const std::shared_ptr<AST::InitVal> &node) {
        return std::get<std::shared_ptr<AST::Exp>>(node->get_value())->addExp();
    }
};


namespace Mir::Symbol {
class Table;
}


namespace Mir::Init {
class Init;
class Array;

template<typename TVal>
std::vector<std::shared_ptr<Init>> flatten_array(const std::shared_ptr<Type::Type> &type,
                                                 const std::shared_ptr<TVal> &initVal,
                                                 const std::shared_ptr<Symbol::Table> &table, bool is_constant,
                                                 const Builder *builder = nullptr
);

std::shared_ptr<Array> fold_array(const std::shared_ptr<Type::Type> &type,
                                  const std::vector<std::shared_ptr<Init>> &flattened_init_values);

class Init : public std::enable_shared_from_this<Init> {
protected:
    std::shared_ptr<Type::Type> type;

public:
    explicit Init(const std::shared_ptr<Type::Type> &type) : type{type} {}

    virtual ~Init() = default;

    [[nodiscard]] virtual bool is_constant_init() const { return false; }
    [[nodiscard]] virtual bool is_exp_init() const { return false; }
    [[nodiscard]] virtual bool is_array_init() const { return false; }

    [[nodiscard]] virtual std::string to_string() const = 0;
};

class Constant final : public Init {
    std::shared_ptr<Const> const_value;

public:
    explicit Constant(const std::shared_ptr<Type::Type> &type, const std::shared_ptr<Const> &const_value)
        : Init{type}, const_value{const_value} {}

    [[nodiscard]] bool is_constant_init() const override { return true; }
    [[nodiscard]] std::shared_ptr<Const> get_const_value() const { return const_value; }

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
    explicit Exp(const std::shared_ptr<Type::Type> &type, const std::shared_ptr<Value> &exp_value)
        : Init{type}, exp_value{exp_value} {}

    [[nodiscard]] bool is_exp_init() const override { return true; }

    [[nodiscard]] std::shared_ptr<Value> get_exp_value() const { return exp_value; }

    void gen_store_inst(const std::shared_ptr<Value> &addr, const std::shared_ptr<Block> &block);

    [[nodiscard]] std::string to_string() const override {
        log_error("ExpInit cannot be output as a string");
    }

    static std::shared_ptr<Init> create_exp_init_value(const std::shared_ptr<Type::Type> &type,
                                                       const std::shared_ptr<Value> &exp_value);
};

class Array final : public Init {
    std::vector<std::shared_ptr<Init>> init_values;
    std::vector<std::shared_ptr<Init>> flattened_init_values;

public:
    explicit Array(const std::shared_ptr<Type::Type> &type,
                   const std::vector<std::shared_ptr<Init>> &flattened_init_values)
        : Init{type}, flattened_init_values{flattened_init_values} {}

    [[nodiscard]] bool is_array_init() const override { return true; }
    [[nodiscard]] std::vector<std::shared_ptr<Init>> get_flattened_init_values() const { return flattened_init_values; }
    [[nodiscard]] size_t get_size() const { return flattened_init_values.size(); }
    [[nodiscard]] std::shared_ptr<Init> get_init_value(const int idx) const { return init_values[idx]; }
    void add_init_value(const std::shared_ptr<Init> &init_value) { init_values.emplace_back(init_value); }

    template<typename TVal>
    static std::shared_ptr<Array> create_array_init_value(const std::shared_ptr<Type::Type> &type,
                                                          const std::shared_ptr<TVal> &initVal,
                                                          const std::shared_ptr<Symbol::Table> &table,
                                                          const bool is_constant,
                                                          const Builder *builder = nullptr) {
        if (!type->is_array()) {
            log_error("%s is not an array type", type->to_string().c_str());
        }
        const auto &flattened = flatten_array<TVal>(type, initVal, table, is_constant, builder);
        const auto &folded = fold_array(type, flattened);
        return folded;
    }

    static std::shared_ptr<Array> create_zero_array_init_value(const std::shared_ptr<Type::Type> &type);

    void gen_store_inst(const std::shared_ptr<Value> &addr, const std::shared_ptr<Block> &block,
                        const std::vector<int> &dimensions);

    [[nodiscard]] std::string to_string() const override;
};

template<typename TVal>
std::vector<std::shared_ptr<Init>> flatten_array(const std::shared_ptr<Type::Type> &type,
                                                 const std::shared_ptr<TVal> &initVal,
                                                 const std::shared_ptr<Symbol::Table> &table,
                                                 bool is_constant,
                                                 const Builder *const builder) {
    using Trait = InitValTrait<TVal>;
    if (!type->is_array()) {
        log_error("%s is not an array type", type->to_string().c_str());
    }
    if (!Trait::is_array_vals(initVal)) {
        log_error("Not an array");
    }
    const auto &array_type = std::dynamic_pointer_cast<Type::Array>(type);
    const auto cur_dim_size = array_type->get_size();
    const auto &element_type = array_type->get_element_type();
    const auto element_cnt = element_type->is_array()
                                 ? std::dynamic_pointer_cast<Type::Array>(element_type)->get_flattened_size()
                                 : 1;
    const auto &atomic_type = array_type->get_atomic_type();

    std::vector<std::shared_ptr<Init>> flattened;
    flattened.reserve(cur_dim_size * element_cnt);

    for (const auto &val: Trait::get_array_vals(initVal)) {
        if (Trait::is_exp(val)) {
            if (is_constant) {
                flattened.emplace_back(
                    Constant::create_constant_init_value(atomic_type, Trait::get_addExp(val), table));
            } else {
                const auto &exp_value = builder->visit_addExp(Trait::get_addExp(val));
                flattened.emplace_back(
                    Exp::create_exp_init_value(atomic_type, exp_value));
            }
        } else if (Trait::is_array_vals(val)) {
            // 补零对齐
            const auto pos = flattened.size();
            for (auto i = 0lu; i < (element_cnt - pos % element_cnt) % element_cnt; ++i) {
                flattened.emplace_back(Constant::create_zero_constant_init_value(atomic_type));
            }
            auto sub = flatten_array<TVal>(element_type, val, table, is_constant);
            flattened.insert(flattened.end(), sub.begin(), sub.end());
        }
    }
    // 补足零
    for (auto i = flattened.size(); i < cur_dim_size * element_cnt; ++i) {
        flattened.emplace_back(Constant::create_zero_constant_init_value(atomic_type));
    }
    return flattened;
}

inline std::shared_ptr<Array> fold_array(const std::shared_ptr<Type::Type> &type,
                                         const std::vector<std::shared_ptr<Init>> &flattened_init_values) {
    if (!type->is_array()) {
        log_error("%s is not an array type", type->to_string().c_str());
    }
    const auto array = std::make_shared<Array>(type, flattened_init_values);
    const auto &array_type = std::dynamic_pointer_cast<Type::Array>(type);
    const auto cur_dim_size = array_type->get_size();

    // 计算每个子分块大小
    if (const auto length = flattened_init_values.size() / cur_dim_size; length == 1) {
        // 只有一层
        for (const auto &val: flattened_init_values) {
            array->add_init_value(val);
        }
    } else {
        // 仍然是多维，需要再 fold
        const auto element_type = array_type->get_element_type();
        for (size_t i = 0; i < cur_dim_size; ++i) {
            const long start = static_cast<long>(i * length);
            const long end = static_cast<long>((i + 1) * length);
            array->add_init_value(
                fold_array(
                    element_type,
                    std::vector(
                        flattened_init_values.begin() + start,
                        flattened_init_values.begin() + end
                    )
                )
            );
        }
    }
    return array;
}
}

#endif
