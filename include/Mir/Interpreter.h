#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <cstring>
#include <unordered_map>
#include <utility>

#include "Eval.h"
#include "Structure.h"

namespace Mir {
class Alloc;
struct Interpreter {
    struct Key {
        std::string func_name;
        std::vector<eval_t> func_args;

        Key(std::string name, const std::vector<eval_t> &args) : func_name(std::move(name)), func_args(args) {}

        bool operator==(const Key &other) const { return func_name == other.func_name && func_args == other.func_args; }

        bool operator!=(const Key &other) const { return !this->operator==(other); }

        struct Hash {
            std::size_t operator()(const Key &key) const {
                std::size_t hash = std::hash<std::string>{}(key.func_name);
                for (const auto &arg: key.func_args) {
                    const size_t arg_hash =
                            std::visit([](auto &&v) { return std::hash<std::decay_t<decltype(v)>>{}(v); }, arg);
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
        size_t size() const {
            return cache_map.size();
        }

        void clear() { cache_map.clear(); }

        [[nodiscard]]
        bool contains(const Key &key) {
            return cache_map.find(key) != cache_map.end();
        }

        [[nodiscard]]
        eval_t get(const Key &key) const {
            if (cache_map.find(key) != cache_map.end()) {
                return cache_map.at(key);
            }
            log_error("Cache miss");
        }
    };

    class Memory {
    public:
        std::vector<std::byte> storage;
        size_t next_alloc_ptr = 0;

        size_t allocate(const size_t size, const size_t alignment) {
            if (const size_t remainder = next_alloc_ptr % alignment; remainder != 0) {
                next_alloc_ptr += alignment - remainder;
            }
            const size_t addr = next_alloc_ptr;
            next_alloc_ptr += size;
            if (storage.size() < next_alloc_ptr) {
                storage.resize(next_alloc_ptr);
            }
            return addr;
        }

        template<typename T>
        void write(const size_t addr, T value) {
            if (addr + sizeof(T) > storage.size()) {
                throw std::out_of_range("Memory write operation is out of bounds.");
            }
            std::memcpy(&storage[addr], &value, sizeof(T));
        }

        void zero_fill(const size_t addr, const size_t size) {
            if (addr + size > storage.size()) {
                throw std::out_of_range("Memory zero_fill operation is out of bounds.");
            }
            std::memset(&storage[addr], 0, size);
        }

        template<typename T>
        T read(const size_t addr) const {
            if (addr + sizeof(T) > storage.size()) {
                throw std::out_of_range("Memory read operation is out of bounds.");
            }

            T result;
            std::memcpy(&result, &storage[addr], sizeof(T));
            return result;
        }
    };

    struct Frame {
        // 函数解释执行的返回值
        eval_t ret_value{0};
        // 当前位于的基本块
        std::shared_ptr<Block> current_block{nullptr};
        // 上一个基本块
        std::shared_ptr<Block> prev_block{nullptr};
        std::unordered_map<const Value *, eval_t> value_map{}, phi_cache{};

        // valid if in main_mode
        Memory memory;
        std::vector<std::shared_ptr<Instruction>> kept;
    };

    std::shared_ptr<Frame> frame{nullptr};

    std::weak_ptr<Cache> cache;

    explicit Interpreter(const std::shared_ptr<Cache> &cache, const bool module_mode = false) :
        cache(cache), module_mode(module_mode) {}

    [[nodiscard]]
    eval_t get_runtime_value(Value *value) const;

    [[nodiscard]]
    eval_t get_runtime_value(const std::shared_ptr<Value> &value) const {
        return get_runtime_value(value.get());
    }

    static void abort();

    void interpret_module_mode(const std::shared_ptr<Function> &main_func);

    void interpret_function(const std::shared_ptr<Function> &func, const std::vector<eval_t> &real_args);

    void interpret_instruction(const std::shared_ptr<Instruction> &instruction);

    [[nodiscard]] bool is_module_mode() const { return module_mode; }

private:
    static size_t counter_limit;
    // 程序计数器
    size_t counter{0};

    bool module_mode;
};
} // namespace Mir

#endif
