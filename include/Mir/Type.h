#ifndef TYPE_H
#define TYPE_H
#include <memory>
#include <string>
#include "Utils/Token.h"


namespace Mir::Type {
class Type : public std::enable_shared_from_this<Type> {
public:
    virtual ~Type() = default;

    [[nodiscard]] virtual bool is_array() const { return false; }
    [[nodiscard]] virtual bool is_integer() const { return false; }
    [[nodiscard]] virtual bool is_int32() const { return false; }
    [[nodiscard]] virtual bool is_int1() const { return false; }
    [[nodiscard]] virtual bool is_float() const { return false; }
    [[nodiscard]] virtual bool is_pointer() const { return false; }
    [[nodiscard]] virtual bool is_void() const { return false; }
    [[nodiscard]] virtual bool is_label() const { return false; }

    [[nodiscard]] virtual bool _equal(const Type &other) const = 0;

    [[nodiscard]] virtual std::string to_string() const = 0;

    bool operator==(const Type &other) const { return _equal(other); }

    bool operator!=(const Type &other) const { return !(*this == other); }

    template<typename T>
    std::shared_ptr<T> as() {
        static_assert(std::is_base_of_v<Type, T> && !std::is_same_v<Type, T>,
                      "T must be a derived class of Type, not Type itself");
        return std::static_pointer_cast<T>(shared_from_this());
    }
};

class Integer final : public Type {
    int bits_;

public:
    static const std::shared_ptr<Integer> i1;
    static const std::shared_ptr<Integer> i8;
    static const std::shared_ptr<Integer> i32;
    static const std::shared_ptr<Integer> i64;
    explicit Integer(const int bits) : bits_{bits} {}
    [[nodiscard]] bool is_integer() const override { return true; }
    [[nodiscard]] bool is_int32() const override { return bits_ == 32; }
    [[nodiscard]] bool is_int1() const override { return bits_ == 1; }
    [[nodiscard]] std::string to_string() const override { return "i" + std::to_string(bits_); }

    [[nodiscard]] int bits() const { return bits_; }

    [[nodiscard]] bool _equal(const Type &other) const override {
        const auto p = dynamic_cast<const Integer *>(&other);
        return p && bits_ == p->bits_;
    }
};

class Float final : public Type {
public:
    static const std::shared_ptr<Float> f32;

    explicit Float() = default;

    [[nodiscard]] bool is_float() const override { return true; }
    [[nodiscard]] std::string to_string() const override { return "float"; }

    [[nodiscard]] bool _equal(const Type &other) const override {
        return dynamic_cast<const Float *>(&other) != nullptr;
    }
};

class Array final : public Type {
    size_t size;
    std::shared_ptr<Type> element_type;

    explicit Array(const size_t size, const std::shared_ptr<Type> &element_type) :
        size{size}, element_type{element_type} {}

public:
    static std::shared_ptr<Array> create(size_t size, const std::shared_ptr<Type> &element_type);

    [[nodiscard]] bool is_array() const override { return true; }

    [[nodiscard]] size_t get_size() const { return size; }

    [[nodiscard]] std::shared_ptr<Type> get_element_type() const { return element_type; }

    // 获取将数组展平后的元素个数
    // e.g. 对于 [2 x [3 x i32]]，该方法返回6
    [[nodiscard]] size_t get_flattened_size() const;

    [[nodiscard]] size_t get_dimensions() const;

    // 获取数组中存在的最小元素类型
    // 返回值只可能为int32或f32的指针
    [[nodiscard]] std::shared_ptr<Type> get_atomic_type() const;

    [[nodiscard]] std::string to_string() const override {
        return "[" + std::to_string(size) + " x " + element_type->to_string() + "]";
    }

    [[nodiscard]] bool _equal(const Type &other) const override {
        const auto p = dynamic_cast<const Array *>(&other);
        return p && size == p->size && *element_type == *(p->element_type);
    }
};

class Pointer final : public Type {
    std::shared_ptr<Type> contain_type;

    explicit Pointer(const std::shared_ptr<Type> &contain_type) : contain_type{contain_type} {}

public:
    static std::shared_ptr<Pointer> create(const std::shared_ptr<Type> &contain_type);

    [[nodiscard]] bool is_pointer() const override { return true; }

    [[nodiscard]] std::shared_ptr<Type> get_contain_type() const { return contain_type; }

    [[nodiscard]] std::string to_string() const override { return contain_type->to_string() + "*"; }

    [[nodiscard]] bool _equal(const Type &other) const override {
        const auto p = dynamic_cast<const Pointer *>(&other);
        return p && *contain_type == *p->contain_type;
    }
};

class Void final : public Type {
public:
    static const std::shared_ptr<Void> void_;

    explicit Void() = default;

    [[nodiscard]] bool is_void() const override { return true; }
    [[nodiscard]] std::string to_string() const override { return "void"; }

    [[nodiscard]] bool _equal(const Type &other) const override {
        return dynamic_cast<const Void *>(&other) != nullptr;
    }
};

class Label final : public Type {
public:
    static const std::shared_ptr<Label> label;

    explicit Label() = default;

    [[nodiscard]] bool is_label() const override { return true; }
    [[nodiscard]] std::string to_string() const override { return "label"; }

    [[nodiscard]] bool _equal(const Type &other) const override {
        return dynamic_cast<const Label *>(&other) != nullptr;
    }
};

[[nodiscard]] std::shared_ptr<Type> get_type(const Token::Type &token_type);
}
#endif
