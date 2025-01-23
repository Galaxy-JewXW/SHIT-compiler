#include <algorithm>
#include <memory>
#include <vector>

#include "Mir/Value.h"

namespace Mir {
void Value::add_user(const std::shared_ptr<User> &user) {
    cleanup_users();
    users_.push_back(user);
}

void Value::delete_user(const std::shared_ptr<User> &user) {
    cleanup_users();
    users_.erase(
        std::remove_if(users_.begin(), users_.end(),
                       [&user](const std::weak_ptr<User> &wp) {
                           const auto sp = wp.lock();
                           return sp && sp.get() == user.get();
                       }),
        users_.end());
}

void Value::cleanup_users() {
    users_.erase(
        std::remove_if(users_.begin(), users_.end(),
                       [](const std::weak_ptr<User> &wp) {
                           return wp.expired();
                       }),
        users_.end());
}

void Value::replace_by_new_value(const std::shared_ptr<Value> &new_value) {
    for (auto &user: users_) {
        if (const auto sp = user.lock()) {
            sp->modify_operand(shared_from_this(), new_value);
        }
    }
    users_.clear();
}

void User::add_operand(const std::shared_ptr<Value> &value) {
    operands_.push_back(value);
    if (value) {
        value->add_user(std::dynamic_pointer_cast<User>(shared_from_this()));
    }
}

void User::clear_operands() {
    for (const auto &operand: operands_) {
        operand->delete_user(std::dynamic_pointer_cast<User>(shared_from_this()));
    }
}

void User::modify_operand(const std::shared_ptr<Value> &old_value,
                          const std::shared_ptr<Value> &new_value) {
    for (auto &operand: operands_) {
        if (operand == old_value) {
            operand->delete_user(std::dynamic_pointer_cast<User>(shared_from_this()));
            operand = new_value;
            operand->add_user(std::dynamic_pointer_cast<User>(shared_from_this()));
        }
    }
}
}
