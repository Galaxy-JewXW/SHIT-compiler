#ifndef VALUE_H
#define VALUE_H

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Type.h"

namespace Mir {
class User;

class Value : public std::enable_shared_from_this<Value> {
protected:
    std::string name_;
    std::shared_ptr<Type::Type> type_;
    std::vector<std::weak_ptr<User>> users_{};

    // Value对应的User被销毁后，在users_中可能依然存有对该user的指针
    // 因此需要在增删user时清理users_，防止出现访存异常
    void cleanup_users();

public:
    Value(std::string name, const std::shared_ptr<Type::Type> &type)
        : name_{std::move(name)}, type_(type) {}

    Value(const Value &other)
        : Value(other.name_, other.type_) {
        std::for_each(other.users_.begin(), other.users_.end(), [this](const auto &user_wp) {
            if (auto user_sp = user_wp.lock()) {
                add_user(user_sp);
            }
        });
    }

    Value &operator=(const Value &other) {
        if (this != &other) {
            name_ = other.name_;
            type_ = other.type_;
            std::for_each(other.users_.begin(), other.users_.end(), [this](const auto &user_wp) {
                if (auto user_sp = user_wp.lock()) {
                    add_user(user_sp);
                }
            });
        }
        return *this;
    }

    virtual ~Value() = default;

    [[nodiscard]] const std::string &get_name() const { return name_; }

    void set_name(const std::string &name) { this->name_ = name; }

    [[nodiscard]] std::shared_ptr<Type::Type> get_type() const { return type_; }

    void add_user(const std::shared_ptr<User> &user);

    void delete_user(const std::shared_ptr<User> &user);

    void replace_by_new_value(const std::shared_ptr<Value> &new_value);

    [[nodiscard]] virtual bool is_constant() { return false; }

    [[nodiscard]] virtual std::string to_string() const = 0;
};

class User : public Value {
protected:
    std::vector<std::shared_ptr<Value>> operands_;

public:
    User(const std::string &name, const std::shared_ptr<Type::Type> &type)
        : Value{name, type} {}

    User(const User &other)
        : Value(other) {
        std::for_each(other.operands_.begin(), other.operands_.end(), [this](const auto &operand) {
            if (operand) { add_operand(operand); }
        });
    }

    User &operator=(const User &other) {
        if (this != &other) {
            Value::operator=(other);
            clear_operands();
            std::for_each(other.operands_.begin(), other.operands_.end(), [this](const auto &operand) {
                if (operand) { add_operand(operand); }
            });
        }
        return *this;
    }

    ~User() override {
        for (const auto &operand: operands_) {
            operand->delete_user(std::shared_ptr<User>(this, [](User *) {}));
        }
        operands_.clear();
    }

    [[nodiscard]] const std::vector<std::shared_ptr<Value>> &get_operands() const { return operands_; }

    void add_operand(const std::shared_ptr<Value> &value);

    void clear_operands();

    void modify_operand(const std::shared_ptr<Value> &old_value, const std::shared_ptr<Value> &new_value);
};
}

#endif
