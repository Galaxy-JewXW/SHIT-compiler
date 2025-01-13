#include <algorithm>
#include <memory>
#include <vector>

#include "Mir/Value.h"

namespace Mir {
[[nodiscard]] std::vector<std::shared_ptr<Use>> Value::get_uses() const {
    std::vector<std::shared_ptr<Use>> result;
    for (const auto &weak_use: uses_) {
        if (auto shared_use = weak_use.lock()) {
            result.push_back(shared_use);
        }
    }
    return result;
}

void Value::add_use(const std::shared_ptr<Use> &use) {
    uses_.emplace_back(use);
}

void Value::remove_use(const std::shared_ptr<Use> &use) {
    uses_.erase(std::remove_if(uses_.begin(), uses_.end(),
                               [&](const std::weak_ptr<Use> &weak_use) {
                                   const auto shared_use = weak_use.lock();
                                   return !shared_use || shared_use == use;
                               }),
                uses_.end());
}

Use::~Use() {
    if (const auto value_shared = value_.lock()) {
        value_shared->remove_use(shared_from_this());
    }
}

std::shared_ptr<Use> Use::create(const std::shared_ptr<Value> &value, const std::shared_ptr<User> &user) {
    struct MakeSharedEnabler : Use {
        MakeSharedEnabler(const std::shared_ptr<Value> &v, const std::shared_ptr<User> &u)
            : Use(v, u) {}
    };
    auto use = std::make_shared<MakeSharedEnabler>(value, user);
    if (value) {
        value->add_use(use);
    }
    return use;
}

std::shared_ptr<Value> Use::get_value() const {
    return value_.lock();
}

std::shared_ptr<User> Use::get_user() const {
    return user_.lock();
}

void Use::set_value(const std::shared_ptr<Value> &new_value) {
    if (const auto old_value = value_.lock()) {
        old_value->remove_use(shared_from_this());
    }
    value_ = new_value;
    if (new_value) {
        new_value->add_use(shared_from_this());
    }
}

[[nodiscard]] std::vector<std::shared_ptr<Use>> User::get_operands() const {
    return operands_;
}

void User::add_operand(const std::shared_ptr<Value> &value_ptr) {
    const auto user_shared = shared_from_this();
    auto use = Use::create(value_ptr, user_shared);
    operands_.emplace_back(use);
}

void User::remove_operand(const std::shared_ptr<Use> &use_ptr) {
    operands_.erase(std::remove(operands_.begin(), operands_.end(), use_ptr),
                    operands_.end());
}
}
