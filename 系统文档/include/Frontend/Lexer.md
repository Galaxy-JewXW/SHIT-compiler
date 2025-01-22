# Lexer.h

+ 引用 Compiler.h, Token.h 文件
+ 定义 Lexer 类



## Class Lexer 



### public:



+ Lexer

构造函数：

```c++
explicit Lexer(std::string src) : input(std::move(src)), pos(0), line(1) {}
```

输入文件字符串，初始化列表：字符串，指针位置，行数



+ tokenize

  ```
  std::vector<Token::Token> tokenize()
  ```

  获取分割后的 token 流



### private

#### 成员变量：

+ std::string input, size_t pos, int line, std::vector \<Token::Token\> tokens

#### 成员集合：

+ ```
  std::unordered_set<std::string> keywords
  ```

​	源程序关键词集合，以字符串形式保存

+ ```
  std::unordered_map<std::string, Token::Type> operators
  ```

​	源程序中的运算符与分隔符，以映射形式保存，每个字符串映射与对应的 Token::Type

#### 成员函数：

+ peek()

  查看当前字符 input[pos]

  + 不修改成员变量

  + 其实类似 peek_next 这里也应该做越位检验

+ peek_next()

  查看当前位置的下一个字符 input[pos + 1]

  + 不修改成员变量
  + 如果当前位置的下一字符已经超出限制，则返回 '\0' ,这代表返回的 '\0' 不一定是实际存在于文件中的，可能只代表已经结束了。

+ advance()

​	向前移动一步当前位置（pos++）,同时维护当前行数

​	如果移动前字符为'\n'，移动后已经在下一行，行数 +1

​	最后返回移动前的字符。

+ consume_whitespace()
  + isspace(peek()) 库函数，会跳过'  ' '\\t' '\\n' '\\v' '\\r' '\\f'字符

​	循环调用 advance() 消耗字符，只需要不越界访问或当前字符为空。

​	advance 中自动维护行数，故该函数不会引起行数错误

+  consume_line_comment();
+ consume_block_comment();

+ Token::Token consume_ident_or_keyword();
+ Token::Token consume_number();
+ Token::Token consume_string();
+ Token::Token consume_operator();

+ ```
  static Token::Type string_to_type(const std::string &str)
  ```

​	这里用多个 if 通过传入的 str 返回一个 Token::Type

