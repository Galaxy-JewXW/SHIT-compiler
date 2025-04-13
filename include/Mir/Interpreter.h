#ifndef INTERPRETER_H
#define INTERPRETER_H
#include <memory>
#include <unordered_map>
#include <utility>
#include <variant>

#include "Instruction.h"
#include "Structure.h"

namespace Mir {
struct eval_t : std::variant<int, double> {
    using variant::variant;

    template<typename T>
    explicit operator T() const {
        return std::visit([](auto v) -> T {
            return static_cast<T>(v);
        }, *this);
    }

    explicit eval_t(const size_t v) : variant(static_cast<int>(v)) {}

    friend eval_t operator+(const eval_t &lhs, const eval_t &rhs) {
        return std::visit([](auto a, auto b) -> eval_t {
            using T = std::common_type_t<decltype(a), decltype(b)>;
            return eval_t(static_cast<T>(a) + static_cast<T>(b));
        }, lhs, rhs);
    }

    friend eval_t operator-(const eval_t &lhs, const eval_t &rhs) {
        return std::visit([](auto a, auto b) -> eval_t {
            using T = std::common_type_t<decltype(a), decltype(b)>;
            return eval_t(static_cast<T>(a) - static_cast<T>(b));
        }, lhs, rhs);
    }

    friend eval_t operator*(const eval_t &lhs, const eval_t &rhs) {
        return std::visit([](auto a, auto b) -> eval_t {
            using T = std::common_type_t<decltype(a), decltype(b)>;
            return eval_t(static_cast<T>(a) * static_cast<T>(b));
        }, lhs, rhs);
    }

    friend eval_t operator/(const eval_t &lhs, const eval_t &rhs) {
        return std::visit([](auto a, auto b) -> eval_t {
            using T = std::common_type_t<decltype(a), decltype(b)>;
            if (static_cast<T>(b) == 0) {
                log_error("Division by zero");
            }
            return eval_t(static_cast<T>(a) / static_cast<T>(b));
        }, lhs, rhs);
    }

    friend eval_t operator%(const eval_t &lhs, const eval_t &rhs) {
        return std::visit([](auto a, auto b) -> eval_t {
            if (b == 0) {
                log_error("Modulus by zero");
            }
            if constexpr (std::is_integral_v<decltype(a)> && std::is_integral_v<decltype(b)>) {
                return eval_t(a % b);
            } else {
                return std::fmod(a, b);
            }
        }, lhs, rhs);
    }

    friend int operator>=(const eval_t &lhs, const eval_t &rhs) {
        return std::visit([](auto a, auto b) -> int {
            using T = std::common_type_t<decltype(a), decltype(b)>;
            return static_cast<T>(a) >= static_cast<T>(b);
        }, lhs, rhs);
    }

    friend int operator==(const eval_t &lhs, const eval_t &rhs) {
        return std::visit([](auto a, auto b) -> int {
            using T = std::common_type_t<decltype(a), decltype(b)>;
            return static_cast<T>(a) == static_cast<T>(b);
        }, lhs, rhs);
    }

    friend int operator<=(const eval_t &lhs, const eval_t &rhs) {
        return std::visit([](auto a, auto b) -> int {
            using T = std::common_type_t<decltype(a), decltype(b)>;
            return static_cast<T>(a) <= static_cast<T>(b);
        }, lhs, rhs);
    }

    friend int operator>(const eval_t &lhs, const eval_t &rhs) {
        return std::visit([](auto a, auto b) -> int {
            using T = std::common_type_t<decltype(a), decltype(b)>;
            return static_cast<T>(a) > static_cast<T>(b);
        }, lhs, rhs);
    }

    friend int operator<(const eval_t &lhs, const eval_t &rhs) {
        return std::visit([](auto a, auto b) -> int {
            using T = std::common_type_t<decltype(a), decltype(b)>;
            return static_cast<T>(a) < static_cast<T>(b);
        }, lhs, rhs);
    }

    friend int operator!=(const eval_t &lhs, const eval_t &rhs) {
        return std::visit([](auto a, auto b) -> int {
            using T = std::common_type_t<decltype(a), decltype(b)>;
            return static_cast<T>(a) != static_cast<T>(b);
        }, lhs, rhs);
    }
};

class ConstexprFuncInterpreter {
public:
    struct Key {
        std::string function_name;
        std::vector<eval_t> arguments;

        Key(std::string function_name, const std::vector<eval_t> &arguments)
            : function_name(std::move(function_name)), arguments(arguments) {}

        bool operator==(const Key &other) const {
            return function_name == other.function_name && arguments == other.arguments;
        }

        bool operator!=(const Key &other) const {
            return !this->operator==(other);
        }

        struct Hash {
            std::size_t operator()(const Key &key) const {
                std::size_t hash = std::hash<std::string>{}(key.function_name);
                for (const auto &arg: key.arguments) {
                    const size_t arg_hash = std::visit([](auto &&v) {
                        return std::hash<std::decay_t<decltype(v)>>{}(v);
                    }, arg);
                    hash ^= arg_hash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                }
                return hash;
            }
        };
    };

    struct Cache {
    private:
        std::unordered_map<Key, eval_t, Key::Hash> cache_map;

    public:
        void put(const Key &key, const eval_t &value) { cache_map[key] = value; }

        size_t size() const { return cache_map.size(); }

        void clear() { cache_map.clear(); }

        bool contains(const Key &key) { return cache_map.find(key) != cache_map.end(); }

        eval_t get(const Key &key) const {
            if (cache_map.find(key) != cache_map.end()) {
                return cache_map.at(key);
            }
            log_error("Cache miss");
        }
    };

    static Cache cache;

    // args是传入的实参
    eval_t interpret_function(const std::shared_ptr<Function> &func,
                              const std::vector<eval_t> &real_args);

private:
    eval_t func_return_value = 0;
    std::shared_ptr<Block> current_block = nullptr;
    std::shared_ptr<Block> prev_block = nullptr;
    std::vector<eval_t> eval_stack{};
    std::unordered_map<std::shared_ptr<Value>, eval_t> value_map{}, phi_cache{};

    eval_t get_runtime_value(const std::shared_ptr<Value> &value);

    void interpret_instruction(const std::shared_ptr<Instruction> &instruction);

    void interpret_alloc(const std::shared_ptr<Alloc> &alloc);

    void interpret_load(const std::shared_ptr<Load> &load);

    void interpret_store(const std::shared_ptr<Store> &store);

    void interpret_gep(const std::shared_ptr<GetElementPtr> &gep);

    void interpret_bitcast(const std::shared_ptr<BitCast> &bitcast);

    void interpret_fptosi(const std::shared_ptr<Fptosi> &fptosi);

    void interpret_sitofp(const std::shared_ptr<Sitofp> &sitofp);

    void interpret_fcmp(const std::shared_ptr<Fcmp> &fcmp);

    void interpret_icmp(const std::shared_ptr<Icmp> &icmp);

    void interpret_zext(const std::shared_ptr<Zext> &zext);

    void interpret_br(const std::shared_ptr<Branch> &branch);

    void interpret_jump(const std::shared_ptr<Jump> &jump);

    void interpret_ret(const std::shared_ptr<Ret> &ret);

    void interpret_call(const std::shared_ptr<Call> &call);

    void interpret_intbinary(const std::shared_ptr<IntBinary> &binary);

    void interpret_floatbinary(const std::shared_ptr<FloatBinary> &binary);

    void interpret_phi(const std::shared_ptr<Phi> &phi);
};
}

#endif //INTERPRETER_H
