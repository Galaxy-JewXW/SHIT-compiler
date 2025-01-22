# AST.h

定义命名空间 AST ，内含语法树中的全部节点类定义

## Node

节点基类

+ 定义纯虚函数 to_string() ，其余子类必须实现

  + ```
    [[nodiscard]]
    ```

## CompUnit

```
// CompUnit -> {Decl | FuncDef}
```

+ 继承 Node 

+ final

+ 成员变量

  compunits_ ： std::vector\<std::variant\<std::shared_ptr\<Decl>, std::shared_ptr\<FuncDef\>>> 

​	联合体向量

+ 成员函数 CompUnit
  + explicit
  + 初始化列表 compunits_{compunits}



## FuncDef

```
// FuncDef -> FuncType Ident '(' [FuncFParam { ',' FuncFParam }] ')' Block
```

+ 继承 Node
+ final
+ 成员变量
  + const Token::Type funcType_ ： 函数返回值类型
    + 由 Btype Token 给出，故声明为 Token::Type 类型
  + const std::string ident_ ： 函数名
  + const std::vector\<std::shared_ptr<FuncFParam\>> funcParams_
  + const std::shared_ptr<Block> block_ ： 函数体

+ 构造函数
+ to_string()

## FuncFParam

```
// FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
```

+ 继承 Node
+ final
+ 成员变量
  + const Token::Type bType_ ： 同上
  + const std::string ident_ ： 同上
  + const std::vector\<std::shared_ptr\<Exp>> exps_ : 数组做参数的维数信息
    + 注意第一维是强制省略的

+ 构造函数
+ to_string() 

## Decl

```
// Decl -> ConstDecl | VarDecl
```

+ 继承 Node
+ Decl() {}

​	protected, 可被子类调用

## VarDecl

```
// VarDecl -> BType VarDef { ',' VarDef } ';'
```

+ 继承 VarDecl
+ final
+ 成员变量
  + const Token::Type bType_ ： 同上
  + const std::vector\<std::shared_ptr\<VarDef>> varDefs_ : VarDef 数组

## VarDef

```
// VarDef -> Ident { '[' ConstExp ']' } ('=' InitVal)
```

+ 继承 Node
+ final
+ 成员变量
  + const std::string ident_ ： 标识符
  + const std::vector\<std::shared_ptr\<ConstExp>> constExps_
  + const std::shared_ptr\<InitVal> initVal_ ：初始化节点
+ VarDefs
+ to_string() 

## ConstDecl

```
// ConstDecl ->  'const' BType ConstDef { ',' ConstDef } ';'
```

+ 继承 Decl
+ final
+ 成员变量
  + const Token::Type bType_ ：同上
  + const std::vector\<std::shared_ptr\<ConstDef>> constDefs_ 

+ ConstDecl
+ to_string()

## ConstDef

```
// ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
```

+ 继承 Node
+ final
+ 成员变量
  + const std::string ident_ ： 同上
  + const std::vector\<std::shared_ptr\<ConstExp>> constExps_ 
  + const std::shared_ptr\<ConstInitVal> constInitVal_ 

+ ConstDef
+ to_string()



## ConstInitVal

```
// ConstInitVal -> ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
```

+ 继承 Node

+ final

+ 成员变量

  + ```
    const std::variant<std::shared_ptr<ConstExp>, std::vector<std::shared_ptr<ConstInitVal>>> value_;
    ```

    + constexp 或者是一串 constinitval

  + 多态构造函数

  + is_constExp() 与 is_constInitVals() 判断类型

+ to_string()



## InitVal

```
// InitVal : Exp | '{' [ InitVal { ',' InitVal } ] '}'
```

+ 继承 Node

+ final

+ 成员变量

  + ```
    const std::variant<std::shared_ptr<Exp>, std::vector<std::shared_ptr<InitVal>>> value_;
    ```

    + 同上

  + 多态构造函数

  + 判断类型函数

+ to_string()

## Block

```
// Block -> '{' { (Decl | Stmt) } '}'
```

+ 继承 Node

+ final

+ 成员变量

  + ```
    const std::vector<std::variant<std::shared_ptr<Decl>, std::shared_ptr<Stmt>>> items_;
    ```

    + 一组联合体向量

+ Block

+ to_string()



## Stmt

```
// Stmt -> LVal '=' Exp ';' | [Exp] ';'  | Block
// | 'if' '(' Cond ')' Stmt [ 'else' Stmt ]
// | 'while' '(' Cond ')' Stmt
// | 'break' ';'
// | 'continue' ';'
// | 'return' [Exp] ';'
// | 'putf' '(' StringConst {',' Exp} ')' ';'
```

+ 继承 Node
+ Stmt() {}



## AssignStmt

```
// Stmt -> LVal '=' Exp ';'
```

+ 继承 Stmt
+ final
+ 成员变量：
  + const std::shared_ptr\<LVal> lval_
  + const std::shared_ptr\<Exp> exp_

+ AssignStmt
+ to_string()



## ExpStmt

```
// Stmt -> | [Exp] ';' 
```

+ 继承 Stmt
+ final
+ 成员变量：
  + const std::shared_ptr\<Exp> exp_

+ ExpStmt
+ to_string()



## BlockStmt

```
// Stmt ->  Block
```

+ 继承 Stmt
+ final
+ 成员变量：
  + const std::shared_ptr\<Block> block_

+ BlockStmt
+ to_string()



## IfStmt 

```
// Stmt ->  'if' '(' Cond ')' Stmt [ 'else' Stmt ]
```

+ 继承 Stmt
+ final
+ 成员变量：
  + const std::shared_ptr\<Cond> cond_
  + const std::shared_ptr\<Stmt> then_
  + const std::shared_ptr\<Stmt> else_

+ IfStmt
+ to_string()



## WhileStmt

```
//Stmt -> 'while' '(' Cond ')' Stmt
```

+ 继承 Stmt
+ final
+ 成员变量：
  + const std::shared_ptr\<Cond> cond_
  + const std::shared_ptr\<Stmt> body_

+ WhileStmt
+ to_string()



## BreakStmt

```
Stmt -> 'break' ';'
```

+ 继承 Stmt
+ final
+ to_string()



## ContinueStmt

```
Stmt -> 'continue' ';'
```

+ 继承 Stmt
+ final
+ to_string()



## ReturnStmt

```
Stmt -> 'return' [Exp] ';'
```

+ 继承 Stmt
+ final
+ 成员变量：
  + const std::shared_ptr\<Exp> exp_
+ ReturnStmt
+ to_string()



## LVal

```cpp
// LVal -> Ident {'[' Exp ']'}
```

+ 继承 Node
+ final
+ 成员变量
  + const std::string ident_;
  + const std::vector\<std::shared_ptr\<Exp>> exps_;
    + exp 的向量

+ LVal
+ to_string()



## Cond

```
// Cond -> LOrExp
```

+ 继承 Node
+ final
+ 成员变量： 
  + const std::shared_ptr\<LOrExp> lOrExp_

+ Cond
+ to_string()



## ConstExp

```
// ConstExp -> AddExp
```

+ 继承 Node
+ final
+ 成员变量：
  +  const std::shared_ptr\<AddExp> addExp_;

+ ConstExp
+ to_string()



## Exp

```
// Exp -> AddExp
```

+ 继承 Node
+ final
+ 成员变量
  + const std::shared_ptr\<AddExp> addExp_;

+ Exp
+ to_string()



## LOrExp

```
// LOrExp -> LAndExp { || LAndExp }
```

+ 继承 Node
+ final
+ 成员变量
  +  const std::vector\<std::shared_ptr\<LAndExp>> lAndExps_;

+ LOrExp
+ to_string()



## LAndExp

```
// LAndExp -> EqExp { && EqExp }
```

+ 继承 Node
+ final
+ 成员变量
  + const std::vector\<std::shared_ptr\<EqExp>> eqExps_;

+ LAndExp
+ to_string()



## EqExp

```
// EqExp -> RelExp { (== | !=) RelExp }
```

+ 继承 Node
+ final
+ 成员变量
  + const std::vector\<std::shared_ptr\<RelExp>> relExps_;
  +  const std::vector\<Token::Type> operators_;

+ EqExp
  + 做数量检验：operators_.size() != relExps_.size() - 1 则抛出异常 
  + 实际上是构造出交错排列的向量结构

+ to_string()



## RelExp

```
// RelExp -> AddExp { (> | < | >= | <=) AddExp }
```

+ 继承 Node
+ final
+ 成员变量：
  + const std::vector\<std::shared_ptr\<AddExp>> addExps_;
  + const std::vector\<Token::Type> operators_;

+ RelExp
  + 数量检验，同上
+ to_string()



## AddExp

```
// AddExp -> MulExp { (+ | -) MulExp }
```

+ 继承 Node
+ final
+ 成员变量：
  + const std::vector\<std::shared_ptr\<MulExp>> mulExps_;
  + const std::vector\<Token::Type> operators_;

+ AddExp
  + 数量检验，同上
+ to_string()



## MulExp

```
// MulExp -> UnaryExp { (* | / | %) UnaryExp}
```

+ 继承 Node
+ final
+ 成员变量：
  + const std::vector\<std::shared_ptr\<UnaryExp>> unaryExps_;
  + const std::vector\<Token::Type> operators_;

+ MulExp
  + 数量检验，同上
+ to_string()



## UnaryExp

```
// UnaryExp -> PrimaryExp | Ident '(' [Exp { ',' Exp }] ')'  | unaryOp UnaryExp
```

+ 继承 Node
+ final
+ 成员变量：
  + using call = std::pair<std::string, std::vector\<std::shared_ptr\<Exp>>>;
    + 别名，表函数调用
  + using opExp = std::pair<Token::Type, std::shared_ptr\<UnaryExp>>;
    + 别名，表一元操作表达式
  + const std::variant<call, opExp, std::shared_ptr\<PrimaryExp>> value_;
    + 联合体

+ 构造函数
+ 类别检验方法
+ to_string()



## PrimaryExp

```
// PrimaryExp -> '(' Exp ')' | LVal | Number | ConstString
```

+ 继承 Node
+ final
+ 成员变量：
  +   const std::variant\<std::shared_ptr\<Exp>, std::shared_ptr\<LVal>, std::shared_ptr\<Number>, std::string> value_;
    + 四种类型的联合
+ 构造函数
+ 类型判断方法
+ to_string()

​	

## Number

+ 继承 Node



## IntNumber

+ 继承 Number 
+ final
+ 成员变量：
  + const int value_
+ IntNumber
+ to_string()



##  FloatNumber

+ 继承 Number
+ final
+ 成员变量：
  + const double value_
+ FloatNumber
+ to_string()
