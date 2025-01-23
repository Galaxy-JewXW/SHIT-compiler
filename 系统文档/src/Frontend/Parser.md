# Parser.cpp

实现 Parser.h 中的方法，将 Token 序列翻译为 AST 中的各个节点结构。

## panic_on

```
bool Parser::panic_on(Types... expected_types)
```

通过 peek() 获取当前待匹配 token

expected_types 为可匹配范围 （FIRST 集），不在其中则抛出异常

如果没有出现异常，则 pos++



## match

```
bool Parser::match(Types... expected_types)
```

同样的检查逻辑

匹配失败则返回 false ，否则返回 true 且 pos++



## parseCompUnit

```
std::shared_ptr<AST::CompUnit> Parser::parseCompUnit()
```

当前不为终结符时 (eof()) 通过前看两个 token 是否为括号，判断是函数还是声明

最后 panic 检验是不是  END_OF_FILE token



## parseDecl

```
std::shared_ptr<AST::Decl> Parser::parseDecl()
```

由当前字符是否为 const Token ，判断是否为 const Decl



## parseConstDecl

```
std::shared_ptr<AST::ConstDecl> Parser::parseConstDecl()
```

panic 检验 const 与 bType

再回看一个字符确定 Btype

能匹配到逗号时一直解析 constDef, 得到 constDecl

最后 panic 检查 分号



## parseConstDef

```
std::shared_ptr<AST::ConstDef> Parser::parseConstDef()
```

panic 检验标识符，再回看确定

如果当前为 [ - token 则匹配索引 exp 直至左侧不是 [ = token

之后 panic 检验 = - token 再匹配 constInitVal



## parseConstInitVal

```
std::shared_ptr<AST::ConstInitVal> Parser::parseConstInitVal()
```

如果当前符号不是 { - token ，说明初始化对应的为单个元素，直接解析 exp 返回即可

否则：

+ 如果{ - token 后紧接 } - token 结束解析 （默认初始化）
+ 否则按文法结构，能解析到 逗号则一直解析下层 constInitVal 



## parseVarDecl

```
std::shared_ptr<AST::VarDecl> Parser::parseVarDecl()
```

panic 检验 bType 再回看一个字符确定 Btype

能匹配到逗号时一直解析 varDef, 得到 varDecl

最后 panic 检查 分号



## parseVarDef

```
std::shared_ptr<AST::VarDef> Parser::parseVarDef()
```

panic 检验标识符，再回看确定

如果当前为 [ - token 则匹配索引 exp 直至左侧不是 [ = token

之后 panic 检验 = - token 再匹配 constInitVal



## parseInitVal

```
std::shared_ptr<AST::InitVal> Parser::parseInitVal()
```

如果当前符号不是 { - token ，说明初始化对应的为单个元素，直接解析 exp 返回即可

否则：

+ 如果{ - token 后紧接 } - token 结束解析 （默认初始化）
+ 否则按文法结构，能解析到 逗号则一直解析下层 constInitVal 



## parseFuncDef

```
std::shared_ptr<AST::FuncDef> Parser::parseFuncDef()
```

先 panic 检验 返回值类型 token 再回看确定

panic 检验标识符，再回看确定

panic 检验左括号，随后若不为右括号（有参数）时，则一直匹配 FuncFParam 直至不为逗号

panic 检验右括号， 再匹配 block



## parseFuncFParam

```
std::shared_ptr<AST::FuncFParam> Parser::parseFuncFParam()
```

先 panic 检验 btype 再回看确定

panic 检验标识符，再回看确定

若此时为 [ - token,则：

+ 第一维默认为空，插入 nullptr ,并 panic 检验 []
+ 此后在不为 ) - token 或 , -token （即参数未结束）时，反复使用 panic 检验[],并匹配中间的 exp

否则直接保持 exps 为空，最后返回



## parseBlock

```
std::shared_ptr<AST::Block> Parser::parseBlock() 
```

panic 检验 { - token

当前不为 } - token 前：

+ 首字符为 bType符，则匹配 Decl
+ 否则 匹配 Stmt



## parseStmt

```
std::shared_ptr<AST::Stmt> Parser::parseStmt()
```

+ break
  + 匹配 break 与 分号
+ continue
  + 匹配 continue 与 分号
+ return: parseReturnStmt
+ if : parseIfStmt
+ WHILE :  parseWhileStmt
+ 若当前 token 为 { = token 则 parseBlock()
+ 如果为标识符（exp / LVal  = exp）
  + 匹配 parseLVal
  + 若后有 = - token 则再匹配 exp，panic 检验分号,得到 AssignStmt
+ 否则只为 exp
  + 如果不是分号（空exp语句）则 parseExp
  + 最后返回一个 ExpStmt



## parseReturnStmt

```
std::shared_ptr<AST::ReturnStmt> Parser::parseReturnStmt()
```

（此时 return token 已经被 match 掉）

当前 token 不是分号 则语句有 exp 部分， parseExp

最后需要 match 分号



## parseIfStmt

```
std::shared_ptr<AST::IfStmt> Parser::parseIfStmt()
```

（此时 if token 已经被 match 掉）

先 panic 检验（ - token 再检验接下来是否为 ) ,是说明 cond 为空，报错

否则匹配 cond, parseCond()

panic 检验 ) token 再 parseStmt() 匹配 thenStmt

检查是否有 else 如果有就再 parseStmt() ，匹配 elseStmt



## parseWhileStmt

```
std::shared_ptr<AST::WhileStmt> Parser::parseWhileStmt()
```

(此时 while token 已经被 match 掉)

先 panic 检验 ( -token 做同上的 )-token 检查，cond 不能为空

之后匹配 Cond ,match )- token,再匹配 body ( parseStmt())



## parseCond

```
std::shared_ptr<AST::Cond> Parser::parseCond() 
```

匹配一个 LOrExp parseLOrExp() ，构造一个 Cond 类型的对象



## parseLVal

```
std::shared_ptr<AST::LVal> Parser::parseLVal() 
```

panic 检查标识符 并回退一个 token 确定

如果能匹配到 [ - token ，则一直匹配 exp 与 ] - token 直到后面无 [ - token



 ## parseExp

```
std::shared_ptr<AST::Exp> Parser::parseExp()
```

匹配一个 AddExp, 构造出 exp



## parseConstExp

```
std::shared_ptr<AST::ConstExp> Parser::parseConstExp()
```

匹配一个 AddExp 构造出一个 exp



## parseLOrExp

```
std::shared_ptr<AST::LOrExp> Parser::parseLOrExp()
```

首先匹配一个 LAndExps

后续有 OR - Token 时一直匹配 LAndExps



## parseLAndExp

```
std::shared_ptr<AST::LAndExp> Parser::parseLAndExp()
```

首先匹配一个 EqExp

后续有  EqExp 时一直匹配  EqExp



## parseEqExp

```
std::shared_ptr<AST::EqExp> Parser::parseEqExp()
```

首先匹配一个 relExp

后续有 EQ/NE 时一直匹配 operator(回看一个 Token) 和 RelExp



## parseRelExp

```
std::shared_ptr<AST::RelExp> Parser::parseRelExp()
```

首先匹配一个 AddExp

后续有 LE/GE/LT/Gt 时一直匹配 operator(回看一个 Token) 和 AddExp



## parseAddExp

```
std::shared_ptr<AST::AddExp> Parser::parseAddExp()
```

首先匹配一个 MulExp

后续有 Add/Sub 时一直匹配 operator(回看一个 token) 和 MulExp



## parseMulExp

```
std::shared_ptr<AST::MulExp> Parser::parseMulExp()
```

首先匹配一个 UnaryExp

后续有 MUL/DIV/MOD 时一直匹配  operator(回看一个 token) 和UnaryExp



## parseUnaryExp

```
std::shared_ptr<AST::UnaryExp> Parser::parseUnaryExp()
```

如果首元素为 Unary Op 则 匹配 op unary

如果首元素为标识符 + 左括号，则为函数调用

+ 匹配函数名与左括号
+ 解析参数列表
  + 若开始解析时为右括号，列表为空，结束
  + 否则当能解析到逗号时就一直解析出 exp 作为实参

否则为 primaryExp



## parsePrimaryExp

如果首元素是整形 token / 浮点数 token ，则 parseNumber

如果是 （ - token ，则为 (exp) ,在匹配 exp 与 ) - token

如果是字符串，匹配

否则是 Lval ,匹配 LVal



## parseNumber

从 parsePrimaryExp 进入 ，但相应 token 已经被 match 过了，这里必须回看一个字符

由类型构造 整数/浮点数