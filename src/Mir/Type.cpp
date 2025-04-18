#include "Mir/Type.h"
#include "Mir/Structure.h"

#include <memory>
#include <unordered_map>

namespace Mir::Type {
const std::shared_ptr<Integer> Integer::i1 = std::make_shared<Integer>(1);
const std::shared_ptr<Integer> Integer::i8 = std::make_shared<Integer>(8);
const std::shared_ptr<Integer> Integer::i32 = std::make_shared<Integer>(32);
const std::shared_ptr<Integer> Integer::i64 = std::make_shared<Integer>(64);
const std::shared_ptr<Float> Float::f32 = std::make_shared<Float>();
const std::shared_ptr<Void> Void::void_ = std::make_shared<Void>();
const std::shared_ptr<Label> Label::label = std::make_shared<Label>();

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


namespace Mir {
class Call;
const std::unordered_map<std::string, std::shared_ptr<Function>> Function::sysy_runtime_functions = {
    {"getint", create("getint", Type::Integer::i32)},
    {"getch", create("getch", Type::Integer::i32)},
    {"getfloat", create("getfloat", Type::Float::f32)},
    {"getarray", create("getarray", Type::Integer::i32, Type::Pointer::create(Type::Integer::i32))},
    {"getfarray", create("getfarray", Type::Integer::i32, Type::Pointer::create(Type::Float::f32))},
    {"putint", create("putint", Type::Void::void_, Type::Integer::i32)},
    {"putch", create("putch", Type::Void::void_, Type::Integer::i32)},
    {"putfloat", create("putfloat", Type::Void::void_, Type::Float::f32)},
    {
        "putarray", create("putarray", Type::Void::void_,
                           Type::Integer::i32, Type::Pointer::create(Type::Integer::i32))
    },
    {
        "putfarray", create("putfarray", Type::Void::void_,
                            Type::Integer::i32, Type::Pointer::create(Type::Float::f32))
    },
    {"putf", create("putf", Type::Void::void_)},
    {"starttime", create("_sysy_starttime", Type::Void::void_, Type::Integer::i32)},
    {"stoptime", create("_sysy_stoptime", Type::Void::void_, Type::Integer::i32)},
};

const std::unordered_map<std::string, std::shared_ptr<Function>> Function::llvm_runtime_functions = {
    {
        "llvm.memset.p0i8.i32", create("llvm.memset.p0i8.i32", Type::Void::void_,
                                       Type::Pointer::create(Type::Integer::i8), Type::Integer::i8,
                                       Type::Integer::i32, Type::Integer::i1)
    },
};
}
