#include <algorithm>
#include <memory>
#include <vector>

#include "Mir/Value.h"
#include "Utils/Log.h"

namespace Mir {
void Value::_add_user(const std::shared_ptr<User> &user) {
    if (!user) return;
    cleanup_users();
    if (std::none_of(users_.begin(), users_.end(),
                     [&user](const std::weak_ptr<User> &wp) {
                         return wp.lock() == user;
                     })) {
        users_.emplace_back(user);
    }
}

void Value::_remove_user(const std::shared_ptr<User> &user) {
    if (!user) return;
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
    if (*type_ != *new_value->get_type()) {
        log_error("type mismatch: expected %s, got %s", type_->to_string().c_str(),
                  new_value->get_type()->to_string().c_str());
    }
    cleanup_users();
    const auto copied_users = users_;
    for (const auto &user: copied_users) {
        if (const auto sp = user.lock()) {
            sp->modify_operand(shared_from_this(), new_value);
        }
    }
    users_.clear();
}

void Value::add_user(const std::shared_ptr<User> &user) {
    if (user) [[likely]] {
        _add_user(user);
        user->_add_operand(shared_from_this());
    }
}

void Value::remove_user(const std::shared_ptr<User> &user) {
    if (user) [[likely]] {
        _remove_user(user);
        user->_remove_operand(shared_from_this());
    }
}

void User::add_operand(const std::shared_ptr<Value> &value) {
    if (value) [[likely]] {
        operands_.push_back(value);
        value->_add_user(std::static_pointer_cast<User>(shared_from_this()));
    }
}

void User::_add_operand(const std::shared_ptr<Value> &value) {
    if (value) [[likely]] {
        operands_.push_back(value);
    }
}

void User::clear_operands() {
    for (const auto &operand: operands_) {
        operand->_remove_user(std::static_pointer_cast<User>(shared_from_this()));
    }
    operands_.clear();
}

void User::remove_operand(const std::shared_ptr<Value> &value) {
    if (!value) return;
    this->operands_.erase(
        std::remove_if(operands_.begin(), operands_.end(),
                       [&value](const std::shared_ptr<Value> &operand) {
                           return operand == value;
                       }),
        operands_.end());
    value->_remove_user(std::static_pointer_cast<User>(shared_from_this()));
}

void User::_remove_operand(const std::shared_ptr<Value> &value) {
    if (!value) return;
    this->operands_.erase(
        std::remove_if(operands_.begin(), operands_.end(),
                       [&value](const std::shared_ptr<Value> &operand) {
                           return operand == value;
                       }),
        operands_.end());
}

void User::modify_operand(const std::shared_ptr<Value> &old_value,
                          const std::shared_ptr<Value> &new_value) {
    if (*old_value->get_type() != *new_value->get_type()) { log_error("type mismatch"); }
    for (auto &operand: operands_) {
        if (operand == old_value) {
            operand->_remove_user(std::static_pointer_cast<User>(shared_from_this()));
            operand = new_value;
            operand->_add_user(std::static_pointer_cast<User>(shared_from_this()));
        }
    }
}
}
