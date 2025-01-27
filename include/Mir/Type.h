#ifndef TYPE_H
#define TYPE_H
#include <memory>
#include <string>
#include "Utils/Token.h"


namespace Mir::Type {
class Type {
public:
    virtual ~Type() = default;

    [[nodiscard]] virtual bool is_array() const { return false; }
    [[nodiscard]] virtual bool is_integer() const { return false; }
    [[nodiscard]] virtual bool is_int32() const { return false; }
    [[nodiscard]] virtual bool is_int1() const { return false; }
    [[nodiscard]] virtual bool is_float() const { return false; }
    [[nodiscard]] virtual bool is_pointer() const { return false; }
    [[nodiscard]] virtual bool is_void() const { return false; }

    [[nodiscard]] virtual std::string to_string() const = 0;

    virtual bool operator==(const Type &other) const { return this->to_string() == other.to_string(); }

    virtual bool operator!=(const Type &other) const { return this->to_string() != other.to_string(); }
};

class Integer final : public Type {
    int bits;

public:
    static const std::shared_ptr<Integer> i1;
    static const std::shared_ptr<Integer> i32;
    explicit Integer(const int bits) : bits{bits} {}
    [[nodiscard]] bool is_integer() const override { return true; }
    [[nodiscard]] bool is_int32() const override { return bits == 32; }
    [[nodiscard]] bool is_int1() const override { return bits == 1; }
    [[nodiscard]] std::string to_string() const override { return "i" + std::to_string(bits); }
};

class Float final : public Type {
public:
    static const std::shared_ptr<Float> f32;

    explicit Float() = default;

    [[nodiscard]] bool is_float() const override { return true; }
    [[nodiscard]] std::string to_string() const override { return "float"; }
};

class Array final : public Type {
    size_t size;
    std::shared_ptr<Type> element_type;

public:
    explicit Array(const size_t size, const std::shared_ptr<Type> &element_type) :
        size{size}, element_type{element_type} {}

    [[nodiscard]] bool is_array() const override { return true; }

    [[nodiscard]] size_t get_size() const { return size; }

    [[nodiscard]] std::shared_ptr<Type> get_element_type() const { return element_type; }

    // 获取将数组展平后的元素个数
    // e.g. 对于 [2 x [3 x i32]]，该方法返回6
    [[nodiscard]] size_t get_flattened_size() const;

    // 获取数组中存在的最小元素类型
    // 返回值只可能为int32或f32的指针
    [[nodiscard]] std::shared_ptr<Type> get_atomic_type() const;

    [[nodiscard]] std::string to_string() const override {
        return "[" + std::to_string(size) + " x " + element_type->to_string() + "]";
    }
};

class Pointer final : public Type {
    std::shared_ptr<Type> contain_type;

public:
    explicit Pointer(const std::shared_ptr<Type> &contain_type) : contain_type{contain_type} {}

    [[nodiscard]] bool is_pointer() const override { return true; }

    [[nodiscard]] std::shared_ptr<Type> get_contain_type() const { return contain_type; }

    [[nodiscard]] std::string to_string() const override { return contain_type->to_string() + "*"; }
};

class Void final : public Type {
public:
    static const std::shared_ptr<Void> void_;

    explicit Void() = default;

    [[nodiscard]] bool is_void() const override { return true; }

    [[nodiscard]] std::string to_string() const override { return "void"; }
};

class Label final : public Type {
public:
    static const std::shared_ptr<Label> label;

    explicit Label() = default;

    [[nodiscard]] std::string to_string() const override { return "label"; }
};

[[nodiscard]] std::shared_ptr<Type> get_type(const Token::Type &token_type);
}
#endif
