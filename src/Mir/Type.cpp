#include "Mir/Type.h"
#include "Mir/Structure.h"

#include <unordered_map>

namespace Mir::Type {
const std::shared_ptr<Integer> Integer::i1 = std::make_shared<Integer>(1);
const std::shared_ptr<Integer> Integer::i8 = std::make_shared<Integer>(8);
const std::shared_ptr<Integer> Integer::i32 = std::make_shared<Integer>(32);
const std::shared_ptr<Integer> Integer::i64 = std::make_shared<Integer>(64);
const std::shared_ptr<Float> Float::f32 = std::make_shared<Float>();
const std::shared_ptr<Void> Void::void_ = std::make_shared<Void>();
const std::shared_ptr<Label> Label::label = std::make_shared<Label>();

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
const std::unordered_map<std::string, std::shared_ptr<Function>> Function::runtime_functions = {
    {"getint", create("getint", Type::Integer::i32)},
    {"getch", create("getch", Type::Integer::i32)},
    {"getfloat", create("getfloat", Type::Float::f32)},
    {"getarray", create("getarray", Type::Integer::i32, std::make_shared<Type::Pointer>(Type::Integer::i32))},
    {"getfarray", create("getfarray", Type::Float::f32, std::make_shared<Type::Pointer>(Type::Float::f32))},
    {"putint", create("putint", Type::Void::void_, Type::Integer::i32)},
    {"putch", create("putch", Type::Void::void_, Type::Integer::i32)},
    {"putfloat", create("putfloat", Type::Void::void_, Type::Float::f32)},
    {
        "putarray", create("putarray", Type::Void::void_,
                           Type::Integer::i32, std::make_shared<Type::Pointer>(Type::Integer::i32))
    },
    {
        "putfarray", create("putfarray", Type::Void::void_,
                            Type::Integer::i32, std::make_shared<Type::Pointer>(Type::Float::f32))
    },
    {"putf", create("putf", Type::Void::void_)},
    {"starttime", create("_sysy_starttime", Type::Void::void_, Type::Integer::i32)},
    {"stoptime", create("_sysy_stoptime", Type::Void::void_, Type::Integer::i32)},
    {"llvm.memset.p0i8.i64", create("llvm.memset.p0i8.i64", Type::Void::void_,
        std::make_shared<Type::Pointer>(Type::Integer::i8), Type::Integer::i8, Type::Integer::i64, Type::Integer::i1)},
};
}
