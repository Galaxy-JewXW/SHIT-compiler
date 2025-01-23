# Value.h

定义命名空间 Mir ,以及相关类 Value,Use,User



## Value

+ 成员变量
  + std::vector\<std::weak_ptr\<User>> users_
    + 一组 user 的向量
  + std::string name_
    + value 的名称
  + std::shared_ptr\<Type::Type> type_
    + value 的类型
+ 成员函数
  + Value()
  + get_name()
    + [[nodiscard]]
    + const
  + set_name
  + get_type
    + [[nodiscard]]
    + const
  + get_uses()
    + [[nodiscard]]
    + const
  + cleanup_users
  + add_user
  + remove_user
  + replace_by_new_value

## User

+ 继承 Value
+ 成员变量
  + std::vector\<std::shared_ptr<Value>> operands_
+ 构造函数
+ 成员函数：
  + get_operands
    + [[nodiscard]]
  + add_operand
  + remove_operand
  + clear_operands
  + modify_operand
