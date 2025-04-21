#include "Mir/Type.h"
#include "Mir/Structure.h"

#include <memory>
#include <unordered_map>

namespace Mir::Type {
std::shared_ptr<Array> Array::create(const size_t size, const std::shared_ptr<Type> &element_type) {
    struct Key {
        size_t size;
        std::shared_ptr<Type> element_type;

        bool operator==(const Key &other) const {
            return size == other.size && element_type == other.element_type;
        }
    };

    struct KeyHash {
        size_t operator()(const Key &key) const {
            return std::hash<size_t>{}(key.size) ^
                   std::hash<std::shared_ptr<Type>>{}(key.element_type);
        }
    };
    static std::unordered_map<Key, std::weak_ptr<Array>, KeyHash> cache;
    const Key key{size, element_type};
    if (const auto it = cache.find(key); it != cache.end()) {
        if (auto existing = it->second.lock()) {
            return existing;
        }
    }
    const auto new_array = std::shared_ptr<Array>(new Array(size, element_type));
    cache[key] = new_array;
    return new_array;
}

std::shared_ptr<Pointer> Pointer::create(const std::shared_ptr<Type> &contain_type) {
    static std::unordered_map<std::shared_ptr<Type>, std::weak_ptr<Pointer>> cache;
    if (const auto it = cache.find(contain_type); it != cache.end()) {
        if (auto existing = it->second.lock()) {
            return existing;
        }
    }
    const auto new_pointer = std::shared_ptr<Pointer>(new Pointer(contain_type));
    cache[contain_type] = new_pointer;
    return new_pointer;
}


std::shared_ptr<Type> Array::get_atomic_type() const {
    std::shared_ptr<Type> current = element_type;
    while (current->is_array()) {
        current = std::static_pointer_cast<Array>(current)->get_element_type();
    }
    return current;
}

size_t Array::get_flattened_size() const {
    size_t result = size;
    std::shared_ptr<Type> current = element_type;
    while (current->is_array()) {
        result *= std::static_pointer_cast<Array>(current)->get_size();
        current = std::static_pointer_cast<Array>(current)->get_element_type();
    }
    return result;
}

size_t Array::get_dimensions() const {
    size_t result = 1;
    auto current = element_type;
    while (current->is_array()) {
        result++;
        current = std::static_pointer_cast<Array>(current)->get_element_type();
    }
    return result;
}


[[nodiscard]] std::shared_ptr<Type> get_type(const Token::Type &token_type) {
    static const std::unordered_map<Token::Type, std::shared_ptr<Type>> type_map = {
        {Token::Type::INT, Integer::i32},
        {Token::Type::FLOAT, Float::f32},
        {Token::Type::VOID, Void::void_}
    };
    const auto it = type_map.find(token_type);
    if (it == type_map.end()) { return nullptr; }
    return it->second;
}
}
