#include "Mir/Structure.h"
#include "Mir/Type.h"

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

std::shared_ptr<Module> Module::instance_ = nullptr;
}
