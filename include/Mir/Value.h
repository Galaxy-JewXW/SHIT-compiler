#ifndef VALUE_H
#define VALUE_H

#include <memory>
#include <vector>

namespace Mir {
class Use;
class User;

class Value {
    std::vector<std::weak_ptr<Use>> uses_;

public:
    virtual ~Value() = default;

    [[nodiscard]] std::vector<std::shared_ptr<Use>> get_uses() const;

    void add_use(const std::shared_ptr<Use> &use);

    void remove_use(const std::shared_ptr<Use> &use);
};

class Use : public std::enable_shared_from_this<Use> {
    std::weak_ptr<Value> value_;
    std::weak_ptr<User> user_;

    Use(const std::shared_ptr<Value> &value, const std::shared_ptr<User> &user)
        : value_(value), user_(user) {}

public:
    ~Use();

    static std::shared_ptr<Use> create(const std::shared_ptr<Value> &value, const std::shared_ptr<User> &user);

    std::shared_ptr<Value> get_value() const;

    std::shared_ptr<User> get_user() const;

    void set_value(const std::shared_ptr<Value> &new_value);
};

class User : public Value, public std::enable_shared_from_this<User> {
    std::vector<std::shared_ptr<Use>> operands_;

public:
    User() = default;

    [[nodiscard]] std::vector<std::shared_ptr<Use>> get_operands() const;

    void add_operand(const std::shared_ptr<Value> &value_ptr);

    void remove_operand(const std::shared_ptr<Use> &use_ptr);
};
}

#endif
