# Value和User

> 作者：zxw
> 
> 最近一次更新于2025/3/25

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

    Value(const Value &other) = delete;

    Value &operator=(const Value &other) = delete;

    Value(Value &&other) = delete;

    Value &operator=(Value &&other) = delete;

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

#### 拷贝构造和赋值构造

Value类禁止了通过拷贝构造或赋值构造来构造新的Value对象。

#### 标识符（名称）

获取Value对象的标识符：

`[[nodiscard]] const std::string &get_name() const { return name_; }`。

修改Value对象的标识符：

`void set_name(const std::string &name) { this->name_ = name; }`

#### 类型

获取Value对象的类型：

`[[nodiscard]] std::shared_ptr<Type::Type> get_type() const { return type_; }`。

#### 使用该Value的User

- 获取当前使用该Value对象的User列表：

    `std::vector<std::weak_ptr<User>> &weak_users() { return users_; }`

    注意：该函数返回的是一个weak_ptr列表，**需要另外判断每个weak_ptr是否有效。**

    **如果需要安全遍历该value的user，建议使用迭代器。**

- 添加使用Value对象的User，执行完成后user与value形成`user-use-value`关系：

    `void add_user(const std::shared_ptr<User> &user)`。

    如果user已经在value的users列表里，**则不会重复添加。**

    **该函数只维护了Value的被use关系，不会维护User的use关系。**

- 删除使用Value对象的User，执行完成后user不再使用value：

    `void delete_user(const std::shared_ptr<User> &user)`。

    **该函数只维护了Value的被use关系，不会维护User的use关系。**

- 清理失效User对象：

    `void cleanup_users()`

    使用`add_user`，`delete_user`与`replace_by_new_value`函数或使用迭代器时，会先行自动调用`cleanup_users`函数。
    
    > Value对应的User被销毁后，在users_中可能依然存有对该user的指针。因此需要在增删user时清理users_，防止出现访存异常。

- 安全遍历value对象的user列表：

    `value.users()`

    该函数会执行`cleanup_users`函数，清除已被free的User对象。

#### 常值
判断某个Value是否为常值（字面量）：

`[[nodiscard]] virtual bool is_constant();`

#### 全局替换

将所有使用该User的Value（old_value）替换为另一个Value（new_value）：

`void replace_by_new_value(const std::shared_ptr<Value> &new_value);`

该函数首先会检查`old_value`与`new_value`是否为同一类型。如果满足该条件，函数会调用`User`类中的`modify_operand`函数，会自动维护old_value与new_value与相关user的使用关系。

**建议后续优化中如果出现value替换的场景，优先使用本函数。**

#### 类型转换

对指向Value对象的shared_ptr进行静态类型转换：

`template<typename T> std::shared_ptr<T> as()`

**注意：该函数在知道对象的真实类型时才可使用**，如当该Value一定是Instruction时，可使用`value.as<Mir::Instruction>()`来减少动态转换带来的开销。其他情况下请使用`std::dynamic_pointer_cast<typename T>`。

## User
在SHIT-complier中，User类的定义如下：

```cpp
class User : public Value {
protected:
    std::vector<std::shared_ptr<Value>> operands_;

public:
    User(const std::string &name, const std::shared_ptr<Type::Type> &type)
        : Value{name, type} {}

    User(const User &other) = delete;

    User &operator=(const User &other) = delete;

    User(User &&other) = delete;

    User &operator=(User &&other) = delete;

    ~User() override {
        for (const auto &operand: operands_) {
            operand->delete_user(std::shared_ptr<User>(this, [](User *) {}));
        }
        operands_.clear();
    }

    const std::vector<std::shared_ptr<Value>> &get_operands() const { return operands_; }

    void add_operand(const std::shared_ptr<Value> &value);

    void clear_operands();

    virtual void modify_operand(const std::shared_ptr<Value> &old_value, const std::shared_ptr<Value> &new_value);

    auto begin() { return operands_.begin(); }
    auto end() { return operands_.end(); }
    auto begin() const { return operands_.begin(); }
    auto end() const { return operands_.end(); }

    void remove_operand(const std::shared_ptr<Value> &value);
};
```

### 数据结构
- `operands_`：User对象使用的Value对象列表。

> 这里使用`std::vector<std::shared_ptr<Value>>`，是因为User持有Value，User控制着Value的生命周期。

### 接口

**与Value类一样，User类不支持拷贝构造和赋值构造**

除先前Value类所拥有的接口之外，User类具有以下的额外接口：

#### User使用的Value（操作数）

- 获取操作数列表

    `const std::vector<std::shared_ptr<Value>> &get_operands() const { return operands_; }`

- 添加操作数

    `void add_operand(const std::shared_ptr<Value> &value);`

    **该函数同时维护了维护了Value的被use关系和User的use关系。**

- 删除操作数

    `void remove_operand(const std::shared_ptr<Value> &value)`

    **该函数同时维护了维护了Value的被use关系和User的use关系。**

- 修改操作数

    `virtual void modify_operand(const std::shared_ptr<Value> &old_value, const std::shared_ptr<Value> &new_value)`

    **该函数同时维护了维护了Value的被use关系和User的use关系。**

    **Phi指令对该函数进行了重写。**

- 清除操作数

    `void clear_operands()`

    **该函数同时维护了维护了Value的被use关系和User的use关系。**

    析构时`~User()`同时会执行`clear_operands`，但是由于可能该User无人使用但尚未析构，建议某一User不再被使用时可以执行`clear_operands`函数。

- 迭代器

    User类通过以下函数实现迭代器：

    ```cpp
    auto begin() { return operands_.begin(); }
    auto end() { return operands_.end(); }
    auto begin() const { return operands_.begin(); }
    auto end() const { return operands_.end(); }
    ```

    通过迭代器可以方便的遍历User对象的操作数列表，示例如下：

    ```cpp
    const std::shared_ptr<Instruction> inst = foo; // Instruction为User的子类
    for (const auto &operand: *inst) { // 将inst进行解引用，获得实际对象，再使用迭代器
        // 执行操作
    }
    ```