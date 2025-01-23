#ifndef INIT_H
#define INIT_H

#include <string>
#include <Utils/Log.h>

#include "Const.h"
#include "Type.h"
#include "Utils/AST.h"

namespace Mir {
namespace Symbol {
    class Table;
}

namespace Init {
    class Init {
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

    // 常数初始值
    class Constant final : public Init {
        std::shared_ptr<Const> const_value;

    public:
        explicit Constant(const std::shared_ptr<Type::Type> &type, const std::shared_ptr<Const> &const_value)
            : Init{type}, const_value{const_value} {}

        [[nodiscard]] bool is_constant_init() const override { return true; }

        [[nodiscard]] std::shared_ptr<Const> get_const_value() const { return const_value; }

        [[nodiscard]] std::string to_string() const override;

        static std::shared_ptr<Constant>
        create_constant_init_value(const std::shared_ptr<Type::Type> &type, const std::shared_ptr<AST::AddExp> &addExp,
                                   const std::shared_ptr<Symbol::Table> &table);

        static std::shared_ptr<Constant> create_zero_constant_init_value(const std::shared_ptr<Type::Type> &type);
    };

    // 一般情况的表达式赋初始值
    class Exp final : public Init {
        std::shared_ptr<Value> exp_value;

    public:
        explicit Exp(const std::shared_ptr<Type::Type> &type, const std::shared_ptr<Value> &exp_value)
            : Init{type}, exp_value{exp_value} {}

        [[nodiscard]] bool is_exp_init() const override { return true; }

        [[nodiscard]] std::shared_ptr<Value> get_exp_value() const { return exp_value; }

        [[nodiscard]] std::string to_string() const override { log_error("ExpInit cannot be output as a string"); }
    };

    // 多维初始值
    class Array final : public Init {
        std::vector<std::shared_ptr<Init>> init_values;
        std::vector<std::shared_ptr<Init>> flattened_init_values;

    public:
        explicit Array(const std::shared_ptr<Type::Type> &type,
                           const std::vector<std::shared_ptr<Init>> &flattened_init_values)
            : Init{type}, flattened_init_values{flattened_init_values} {}

        [[nodiscard]] bool is_array_init() const override { return true; }

        [[nodiscard]] size_t get_size() const { return flattened_init_values.size(); }

        [[nodiscard]] std::shared_ptr<Init> get_init_value(const int idx) const { return init_values[idx]; }

        [[nodiscard]] std::string to_string() const override;

        static std::shared_ptr<Array>
        create_array_init_value(const std::shared_ptr<Type::Type> &type,
                                const std::shared_ptr<AST::ConstInitVal> &constInitVal,
                                const std::shared_ptr<Symbol::Table> &table);

        static std::shared_ptr<Array> create_zero_array_init_value(const std::shared_ptr<Type::Type> &type);
    };
}
}

#endif
