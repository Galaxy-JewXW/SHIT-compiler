# Token.h

定义命名空间 Token 包含一个枚举类 Type,  一个输出函数 type_to_string 和一个 Token 类



## enum class Type

所有的 Token Type 可大致划分为如下几类:

+ 关键词

  ```
  CONST, INT, FLOAT, VOID, IF, ELSE, WHILE, BREAK, CONTINUE, RETURN,
  ```

+ 标识符

  ```
  IDENTIFIER,
  ```

+ 字面量

  ```
  INT_CONST, FLOAT_CONST, STRING_CONST,
  ```

+ 运算符（其实也是一种分隔符，但是表语义，要有保留）

  ```
  ADD, SUB, NOT, MUL, DIV, MOD,
  LT, GT, LE, GE, EQ, NE, AND, OR,
  ```

+ 分隔符

  ```
  SEMICOLON, COMMA, ASSIGN,
  LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
  ```

+ 结束符（表文件结束）

  ```
  END_OF_FILE,
  ```

+ 未知符（只是便于调试的，正常不会出现）

  ```
  UNKNOWN
  ```

  

## type_to_string

将 Tokn :: Type 转换为输出专用的字符串形式，具体实现在 /src/Utils/Print.cpp 中



## class Token

### 成员（均为public）

+ content type,line

  内含字符串，Token Type, 行数

  + 字符串 token 中的 content 无双引号，数字字面量已经转换为十进制无指数形式

    

+ 构造函数 Token

  ```c++
  Token(std::string c, const Type t, const int l)
          : content(std::move(c)), type(t), line(l) {}
  ```

​	参数分别用于初始化对应成员

+ to_string()
  + [[nodiscard]]
  + 不改变成员
  + 虚函数，子类可以实现



