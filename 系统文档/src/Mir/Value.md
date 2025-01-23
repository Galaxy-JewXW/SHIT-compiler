# Value.cpp

对 value.h 中类的各方法做实现



## Value

+ get_uses()

  ```
  [[nodiscard]] std::vector<std::shared_ptr<Use>> Value::get_uses() const
  ```

​	提取有用的对象指针，获取 value 中的 use

+ add_use

  ```
  void Value::add_use(const std::shared_ptr<Use> &use)
  ```

​	向 value 中添加 use

+ remove_use

  ```
  void Value::remove_use(const std::shared_ptr<Use> &use)
  ```

​	移除与参数匹配的项，顺便移除已经销毁的项



## Use

+ ~Use()

  ```
  Use::~Use()
  ```

​	若当前 Use 没有销毁则从其 value 中移除自身（使用 value 的 remove 方法）

+ create

  ```
  std::shared_ptr<Use> Use::create(const std::shared_ptr<Value> &value, const std::shared_ptr<User> &user) 
  ```

​	借助派生类 MakeSharedEnabler 调用 Use 的构造函数，使用参数构造 use

​	之后，调用 value 的 add_use 方法

+ get_value

  ```
  std::shared_ptr<Value> Use::get_value() const
  ```

  返回 value

+ get_user

  ```
  std::shared_ptr<User> Use::get_user() const
  ```

​	返回 user

+ set_value

  ```
  void Use::set_value(const std::shared_ptr<Value> &new_value)
  ```

​	如果原来的 value 依然有效，就先从原来 value 中删去自身

​	改变 value,并在新的 value 上调用 add_use



## User

+ get_operands

  ```
  [[nodiscard]] std::vector<std::shared_ptr<Use>> User::get_operands() const 
  ```

​	返回 user 的所有 use

+ add_operand

  ```
  void User::add_operand(const std::shared_ptr<Value> &value_ptr)
  ```

​	创建一个 use 添加入 operands 中

+ remove_operand

  ```
  void User::remove_operand(const std::shared_ptr<Use> &use_ptr)
  ```

​	从 vector 中去掉这个 use