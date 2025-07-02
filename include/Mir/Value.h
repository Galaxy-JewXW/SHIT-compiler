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
    friend class User;

protected:
    std::string name_;
    std::shared_ptr<Type::Type> type_;
    std::vector<std::weak_ptr<User>> users_{};

public:
    Value(std::string name, const std::shared_ptr<Type::Type> &type)
        : name_{std::move(name)}, type_(type) {}

    Value(const Value &other) = delete;

    Value &operator=(const Value &other) = delete;

    Value(Value &&other) = delete;

    Value &operator=(Value &&other) = delete;

    virtual ~Value() = default;

    [[nodiscard]] const std::string &get_name() const { return name_; }

    void set_name(const std::string &name) { this->name_ = name; }

    void set_type(const std::shared_ptr<Type::Type> &type) { this->type_ = type; }

    [[nodiscard]] std::shared_ptr<Type::Type> get_type() const { return type_; }

    // Value对应的User被销毁后，在users_中可能依然存有对该user的指针
    // 因此需要在增删user时清理users_，防止出现访存异常
    void cleanup_users();

    // 双向维护关系
    void add_user(const std::shared_ptr<User> &user);

    void remove_user(const std::shared_ptr<User> &user);

    template<typename... Args>
    void add_users(Args &&... args) {
        static_assert(
            (std::is_convertible_v<Args, std::shared_ptr<User>> && ...),
            "All arguments must be convertible to std::shared_ptr<User>");
        (add_user(std::forward<Args>(args)), ...);
    }

    template<typename... Args>
    void remove_users(Args &&... args) {
        static_assert(
            (std::is_convertible_v<Args, std::shared_ptr<User>> && ...),
            "All arguments must be convertible to std::shared_ptr<User>");
        (remove_user(std::forward<Args>(args)), ...);
    }

protected:
    // 单向维护关系
    void _remove_user(const std::shared_ptr<User> &user);

    void _add_user(const std::shared_ptr<User> &user);

public:
    void replace_by_new_value(const std::shared_ptr<Value> &new_value);

    [[nodiscard]] virtual bool is_constant() const { return false; }

    [[nodiscard]] virtual std::string to_string() const = 0;

    class UserRange {
        using UserPtr = std::weak_ptr<User>;
        std::vector<UserPtr> &users_;

    public:
        explicit UserRange(std::vector<UserPtr> &users) : users_{users} {}

        struct Iterator {
            std::vector<UserPtr>::iterator current;

            explicit Iterator(const std::vector<UserPtr>::iterator current)
                : current(current) {}

            std::shared_ptr<User> operator*() const { return current->lock(); }

            bool operator!=(const Iterator &other) const {
                return current != other.current;
            }

            Iterator &operator++() {
                ++current;
                return *this;
            }
        };

        [[nodiscard]] std::vector<std::shared_ptr<User>> lock() const {
            std::vector<std::shared_ptr<User>> result;
            result.reserve(users_.size());
            for (const auto &user_weak: users_) {
                if (auto user_shared{user_weak.lock()}) {
                    result.push_back(user_shared);
                }
            }
            return result;
        }

        [[nodiscard]] size_t size() const { return users_.size(); }
        [[nodiscard]] Iterator begin() const { return Iterator{users_.begin()}; }
        [[nodiscard]] Iterator end() const { return Iterator{users_.end()}; }
    };

    [[nodiscard]] UserRange users() {
        cleanup_users();
        return UserRange{users_};
    }

    template<typename T>
    std::shared_ptr<T> as() {
        static_assert(std::is_base_of_v<Value, T>,
                      "T must be a derived class of Value or Value itself");
        return std::static_pointer_cast<T>(shared_from_this());
    }

    template<typename T>
    std::shared_ptr<T> is() {
        static_assert(std::is_base_of_v<Value, T>,
                      "T must be a derived class of Value or Value itself");
        return std::dynamic_pointer_cast<T>(shared_from_this());
    }
};

class User : public Value {
    friend class Value;

protected:
    std::vector<std::shared_ptr<Value>> operands_;

    // 单向维护关系
    void _add_operand(const std::shared_ptr<Value> &value);

    void _remove_operand(const std::shared_ptr<Value> &value);

public:
    User(const std::string &name, const std::shared_ptr<Type::Type> &type)
        : Value{name, type} {}

    User(const User &other) = delete;

    User &operator=(const User &other) = delete;

    User(User &&other) = delete;

    User &operator=(User &&other) = delete;

    ~User() override {
        for (const auto &operand: operands_) {
            operand->_remove_user(std::shared_ptr<User>(this, [](User *) {}));
        }
        operands_.clear();
    }

    const std::vector<std::shared_ptr<Value>> &get_operands() const {
        return operands_;
    }

    // 双向维护关系
    void add_operand(const std::shared_ptr<Value> &value);

    void remove_operand(const std::shared_ptr<Value> &value);

    template<typename... Args>
    void add_operands(Args &&... args) {
        static_assert(
            (std::is_convertible_v<Args, std::shared_ptr<Value>> && ...),
            "All arguments must be convertible to std::shared_ptr<Value>");
        (add_operand(std::forward<Args>(args)), ...);
    }

    template<typename... Args>
    void remove_operands(Args &&... args) {
        static_assert(
            (std::is_convertible_v<Args, std::shared_ptr<Value>> && ...),
            "All arguments must be convertible to std::shared_ptr<Value>");
        (remove_operand(std::forward<Args>(args)), ...);
    }

    void clear_operands();

    virtual void modify_operand(const std::shared_ptr<Value> &old_value, const std::shared_ptr<Value> &new_value);

    auto begin() { return operands_.begin(); }
    auto end() { return operands_.end(); }
    auto begin() const { return operands_.begin(); }
    auto end() const { return operands_.end(); }
};
} // namespace Mir

#endif
