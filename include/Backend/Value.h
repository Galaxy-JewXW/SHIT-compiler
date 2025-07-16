#ifndef BACKEND_VALUE_H
#define BACKEND_VALUE_H

#include <string>
#include <stdexcept>
#include "Mir/Instruction.h"
#include "Backend/VariableTypes.h"

namespace Backend {
    class Operand;
    class Constant;
    class IntValue;
    class FloatValue;
    class Variable;
    class Pointer;
    class Comparison;
    enum class OperandType : uint32_t {
        CONSTANT,
        VARIABLE
    };
    enum class VariableWide : uint32_t {
        GLOBAL,
        FUNCTIONAL,
        LOCAL,
    };
};

class Backend::Operand {
    public:
        std::string name;
        OperandType operand_type;
        Operand(const std::string &name) : name(name), operand_type(OperandType::VARIABLE) {};
        Operand(const std::string &name, OperandType type) : name(name), operand_type(type) {};
        virtual std::string to_string() const { return name; }
};

class Backend::Constant : public Backend::Operand {
    public:
        Backend::VariableType constant_type;
        explicit Constant(const std::string &name, const Backend::VariableType &type) : Operand(name, OperandType::CONSTANT), constant_type(type) {}
        explicit Constant(const Backend::VariableType &type) : Operand("", OperandType::CONSTANT), constant_type(type) {}
};

class Backend::IntValue : public Backend::Constant {
    public:
        int32_t int32_value{0};
        explicit IntValue(const int32_t value) : Backend::Constant(std::to_string(value), Backend::VariableType::INT32), int32_value(value) {};
};

class Backend::FloatValue : public Backend::Constant {
    public:
        const double float_value{0.0};
        explicit FloatValue(const double value) : Backend::Constant(std::to_string(value), Backend::VariableType::FLOAT), float_value(value) {};
};

/*
 * Variable represents a variable(array) object itself.
 * `VariableWide` can only be chosen from below:
 * `GLOBAL`: the **referenced** object is stored in `.data` section.
 * `FUNCTIONAL`: the **referenced** object is to be stored in the function's stack.
 * `LOCAL`: the object is to be stored in physical registers.
 */
class Backend::Variable : public Backend::Operand {
    public:
        enum class Type : uint32_t {
            OBJ, PTR, CMP
        };
        Type var_type{Type::OBJ};
        Backend::VariableType workload_type;
        VariableWide lifetime;
        size_t length{1};
        explicit Variable(const std::string &name, const Backend::VariableType &type, VariableWide lifetime) : Backend::Operand(name, OperandType::VARIABLE), workload_type(type), lifetime(lifetime) {}
        explicit Variable(const std::string &name, const Backend::VariableType &type, VariableWide position, size_t length) : Backend::Operand(name, OperandType::VARIABLE), workload_type(type), lifetime(position), length(length) {}

        [[nodiscard]] inline size_t size() const {
            return Backend::Utils::type_to_size(workload_type) * length;
        }

        bool operator==(const Variable& other) const { return this->name == other.name; }
        bool operator!=(const Variable& other) const { return !(*this == other); }
};

/*
 * Pointer is only used for `GetElementPointer` instructions.
 * `base` should be an address of variable in stack/heap/global_data.
 * `offset` is the offset from the base address, unit is sizeof `base` variable.
 */
class Backend::Pointer : public Backend::Variable {
    public:
        std::shared_ptr<Backend::Variable> base;
        std::shared_ptr<Backend::Operand> offset;
        explicit Pointer(const std::string &name, const std::shared_ptr<Backend::Variable> &base, std::shared_ptr<Backend::Operand> &offset) : Backend::Variable(name, Backend::Utils::to_pointer(base->workload_type), VariableWide::LOCAL), base(base), offset(offset) {
            var_type = Type::PTR;
        }
};

/*
 * CompareVariable is used for comparison operations in MIR (to translate LLVM's ICmp/FCmp).
 * It will not occur in the final MIR, and alive only during the translation phase.
 * After translation, it should be removed.
 */
class Backend::Comparison : public Backend::Variable {
    public:
        enum class Type : uint32_t {
            EQUAL, NOT_EQUAL, GREATER, GREATER_EQUAL, LESS, LESS_EQUAL
        };

        std::shared_ptr<Backend::Variable> lhs;
        std::shared_ptr<Backend::Operand> rhs;
        Type compare_type;

        explicit Comparison(const std::string &name, const std::shared_ptr<Backend::Variable> &lhs, const std::shared_ptr<Backend::Operand> &rhs, Type compare_type) : Backend::Variable(name, Backend::VariableType::INT1, VariableWide::LOCAL), lhs(lhs), rhs(rhs), compare_type(compare_type) {
            var_type = Variable::Type::CMP;
        }
        explicit Comparison(const std::string &name, const std::shared_ptr<Backend::Operand> &lhs, const std::shared_ptr<Backend::Variable> &rhs, Type compare_type) : Backend::Variable(name, Backend::VariableType::INT1, VariableWide::LOCAL), lhs(rhs), rhs(lhs), compare_type(compare_type) {
            var_type = Variable::Type::CMP;
        }

        [[nodiscard]] static Type load_from_llvm(const Mir::Icmp::Op &op) {
            switch (op) {
                case Mir::Icmp::Op::EQ: return Type::EQUAL;
                case Mir::Icmp::Op::NE: return Type::NOT_EQUAL;
                case Mir::Icmp::Op::GT: return Type::GREATER;
                case Mir::Icmp::Op::LT: return Type::LESS;
                case Mir::Icmp::Op::GE: return Type::GREATER_EQUAL;
                case Mir::Icmp::Op::LE: return Type::LESS_EQUAL;
                default: throw std::invalid_argument("Unknown comparison type");
            }
        }
    private:
        static Type to_negation(Type type) {
            switch (type) {
                case Type::GREATER: return Type::LESS;
                case Type::GREATER_EQUAL: return Type::LESS_EQUAL;
                case Type::LESS: return Type::GREATER;
                case Type::LESS_EQUAL: return Type::GREATER_EQUAL;
                default: return type;
            }
        }
};

#endif