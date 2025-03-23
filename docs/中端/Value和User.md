# Value和User

> 作者：zxw
> 
> 最近一次更新于2025/3/24

正如总览中所介绍的，在LLVM中，“一切皆Value”，所有的语法成分均为`Value`的子类。

`User` 类是 `Value` 的一个特殊的子类，是一种可以使用其他 Value 对象的 Value 类。Function、BasicBlock 和 Instruction 都有使用的语法结构，都是 User 的子类，也是 Value 的子类。

## 生命周期

在SHIT-compiler中，我们直接在`User`和`Value`之间维护使用与被使用的关系。这就意味着：**User持有Value**，User控制着Value的生命周期。

因此，Value中存储User的指针为`std::weak_ptr`，只保存弱引用而不会增加使用计数。**因为User持有Value**，所以这样的生命周期控制是合理的。

对应的：User中存储Value的指针为`std::shared_ptr`。

## Value

在SHIT-complier中，Value类的定义如下：
```cpp
class Value : public std::enable_shared_from_this<Value> {
protected:
    std::string name_;
    std::shared_ptr<Type::Type> type_;
    std::vector<std::weak_ptr<User>> users_{};

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

    void cleanup_users();

    void add_user(const std::shared_ptr<User> &user);

    void delete_user(const std::shared_ptr<User> &user);

    void replace_by_new_value(const std::shared_ptr<Value> &new_value);

    std::vector<std::weak_ptr<User>> &weak_users() { return users_; }

    [[nodiscard]] virtual bool is_constant() { return false; }

    [[nodiscard]] virtual std::string to_string() const = 0;

    class UserRange {
        using UserPtr = std::weak_ptr<User>;
        std::vector<UserPtr> &users_;

    public:
        explicit UserRange(std::vector<UserPtr> &users) : users_{users} {}

        struct Iterator {
            std::vector<UserPtr>::iterator current;

            explicit Iterator(const std::vector<UserPtr>::iterator current) : current(current) {}

            std::shared_ptr<User> operator*() const { return current->lock(); }

            bool operator!=(const Iterator &other) const { return current != other.current; }

            Iterator &operator++() {
                ++current;
                return *this;
            }
        };

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
        return std::static_pointer_cast<T>(shared_from_this());
    }
};
```

Value类继承`std::enable_shared_from_this<Value>`：

- 确保对象生命周期：使用 `shared_from_this` 可以获取到自身的 `std::shared_ptr`，从而增加对象的引用计数，避免出现悬空指针和未定义行为。
- 简化内存管理：通过智能指针自动管理对象的生命周期，减少了手动管理内存的复杂性，降低了内存泄漏和双重释放的风险。
- 防止多重所有权：直接在类内部使用 `std::shared_ptr(this)` 可能导致多个独立的 shared_ptr 管理同一对象，造成引用计数不一致。std::enable_shared_from_this 确保了所有的 shared_ptr 共享同一个引用计数。

**在之后的代码中，有关Value的对象均使用智能指针来管理生命周期。**

### 数据结构

Value持有以下的数据：

- `name_`：Value的**名称**（或**标识符**）
- `type_`：Value的**类型**（参见类型系统部分）
- `users_`：使用该Value的User列表

### 接口

#### 拷贝构造函数和赋值构造函数

Value类实现了拷贝构造函数和赋值构造函数，方便对Value进行深拷贝操作。

#### 获取修改标识符

可通过`get_name`与`set_name`方法获取或修改Value对象的标识符。
