#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <unordered_map>

#include "Eval.h"
#include "Structure.h"

namespace Mir {
class Interpreter {
    struct Key {
        std::string func_name;
        std::vector<eval_t> func_args;

        Key(std::string name, std::vector<eval_t> args)
            : func_name(std::move(name)), func_args(std::move(args)) {}

        bool operator==(const Key &other) const {
            return func_name == other.func_name && func_args == other.func_args;
        }

        bool operator!=(const Key &other) const {
            return !this->operator==(other);
        }

        struct Hash {
            std::size_t operator()(const Key &key) const {
                std::size_t hash = std::hash<std::string>{}(key.func_name);
                for (const auto &arg: key.func_args) {
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

        [[nodiscard]]
        size_t size() const { return cache_map.size(); }

        void clear() { cache_map.clear(); }

        [[nodiscard]]
        bool contains(const Key &key) { return cache_map.find(key) != cache_map.end(); }

        [[nodiscard]]
        eval_t get(const Key &key) const {
            if (cache_map.find(key) != cache_map.end()) {
                return cache_map.at(key);
            }
            log_error("Cache miss");
        }
    };

public:
    // 函数解释执行的返回值
    eval_t ret_value{0};
    // 当前位于的基本块
    std::shared_ptr<Block> current_block{nullptr};
    // 上一个基本块
    std::shared_ptr<Block> prev_block{nullptr};
    std::unordered_map<std::shared_ptr<Value>, eval_t> value_map{}, phi_cache{};

    eval_t get_runtime_value(const std::shared_ptr<Value> &value);

    static void interpret_abort();
};
}

#endif
