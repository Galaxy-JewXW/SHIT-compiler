# Value.h

定义命名空间 Mir ,以及相关类 Value,Use,User



## Value

+ 成员变量
  + std::vector\<std::weak_ptr\<Use>> uses
    + 一组 use 的向量
+ 成员函数
  +  ~Value()
  + get_uses()
    +   [[nodiscard]]
    + const
  + add_use
  + remove_use



## Use

+ 成员变量
  + std::weak_ptr\<Value> value_
  + std::weak_ptr\<User> user_
+ 构造函数
+ 析构函数
+ 成员函数：
  + create
  + get_value
  + get_user
  + set_value



## User

+ 继承 Value
+ 成员变量
  + std::vector\<std::shared_ptr<Use>> operands_
+ 构造函数
+ 成员函数：
  + get_operands
    + [[nodiscard]]
  + add_operand
  + remove_operand
