# Print.cpp

定义各数据结构的 to_string 方法

+ Token
+ AST 节点



## type_to_string

```
std::string Token::type_to_string(const Type type)
```

把各类型的 Token 转换成名字字符串



## to_string()

```
[[nodiscard]] std::string Token::Token::to_string() const
```

输出 Token 的行数，类型名与内容

