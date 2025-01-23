# Parser.h

递归下降，分析所有语法成分，构造 AST

## 成员变量

+ const std::vector\<Token::Token> tokens
  + private
+ size_t pos
  + private
  + 当前匹配的 token 位置



## 成员函数

+ peek()
  + const
  + [[nodiscard]]
  + 返回当前匹配到的 token

+ next(const int offset = 1) 
  + const
  + [[nodiscard]]
  + 默认：返回下一个 token
  + 超出 token 序列范围时抛出异常
    + 这里为什么不做检查？

+ 模板方法：
  + panic_on
  + match

+ 解析各语法成分的方法，所有方法都将解析结果作为返回值

+ 构造函数

+ parse

  调用 parseCompUnit() ，并返回解析结果 AST

+ eof()
  + 判断当前解析序列是否已经到达 END_OF_FILE (只在最后出现)