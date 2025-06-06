#include <unordered_map>

#include "Mir/Const.h"

namespace Mir {
std::shared_ptr<ConstBool> ConstBool::create(const int value) {
    const int normalized = value ? 1 : 0;

    struct Key {
        int value;
        bool operator==(const Key &other) const { return value == other.value; }
    };

    struct KeyHash {
        size_t operator()(const Key &key) const { return std::hash<int>{}(key.value); }
    };

    static std::unordered_map<Key, std::weak_ptr<ConstBool>, KeyHash> cache;
    const Key key{normalized};

    if (const auto it = cache.find(key); it != cache.end()) {
        if (auto existing = it->second.lock()) {
            return existing;
        }
    }
    auto new_const = std::shared_ptr<ConstBool>(new ConstBool(normalized));
    cache[key] = new_const;
    return new_const;
}


std::shared_ptr<ConstInt> ConstInt::create(const int value, const std::shared_ptr<Type::Type> &type) {
    struct Key {
        int value;
        std::shared_ptr<Type::Type> type;

        bool operator==(const Key &other) const {
            return value == other.value && type == other.type;
        }
    };

    struct KeyHash {
        size_t operator()(const Key &key) const {
            return std::hash<int>{}(key.value) ^ std::hash<std::shared_ptr<Type::Type>>{}(key.type);
        }
    };

    if (!type->is_integer()) [[unlikely]] {
        log_error("Invalid Integer Type");
    }

    static std::unordered_map<Key, std::weak_ptr<ConstInt>, KeyHash> cache;
    const Key key{value, type};

    if (const auto it = cache.find(key); it != cache.end()) {
        if (auto existing = it->second.lock()) {
            return existing;
        }
    }
    auto new_const = std::shared_ptr<ConstInt>(new ConstInt(value, type));
    cache[key] = new_const;
    return new_const;
}

std::shared_ptr<ConstFloat> ConstFloat::create(const double value) {
    uint64_t bits;
    static_assert(sizeof(value) == sizeof(bits), "Size mismatch");
    std::memcpy(&bits, &value, sizeof(value));

    struct Key {
        uint64_t bits;
        bool operator==(const Key &other) const { return bits == other.bits; }
    };

    struct KeyHash {
        size_t operator()(const Key &key) const { return std::hash<uint64_t>{}(key.bits); }
    };

    static std::unordered_map<Key, std::weak_ptr<ConstFloat>, KeyHash> cache;
    const Key key{bits};

    if (const auto it = cache.find(key); it != cache.end()) {
        if (auto existing = it->second.lock()) {
            return existing;
        }
    }
    auto new_const = std::shared_ptr<ConstFloat>(new ConstFloat(value));
    cache[key] = new_const;
    return new_const;
}
}
