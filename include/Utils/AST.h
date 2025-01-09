#ifndef AST_H
#define AST_H

#include <iostream>
#include <memory>
#include <utility>
#include <variant>
#include <vector>
#include "Utils/Token.h"

namespace AST {
// AST 结点基类
class Node {
    friend class Parser;

public:
    virtual ~Node() = default;
};

class Exp;
class LVal;

class Number : public Node {};

class IntNumber final : public Number {
    const int value_;

public:
    explicit IntNumber(const int &value) : value_{value} {}
};

class FloatNumber final : public Number {
    const float value_;

public:
    explicit FloatNumber(const float &value) : value_{value} {}
};

// PrimaryExp -> '(' Exp ')' | LVal | Number
class PrimaryExp final : public Node {
    const std::variant<std::shared_ptr<Exp>, std::shared_ptr<LVal>, std::shared_ptr<Number>> value_;

public:
    explicit PrimaryExp(const std::shared_ptr<Exp> &exp) : value_{exp} {}

    explicit PrimaryExp(const std::shared_ptr<LVal> &lVal) : value_{lVal} {}

    explicit PrimaryExp(const std::shared_ptr<Number> &number) : value_{number} {}

    [[nodiscard]] bool isExp() const {
        return std::holds_alternative<std::shared_ptr<Exp>>(value_);
    }

    [[nodiscard]] bool isLVal() const {
        return std::holds_alternative<std::shared_ptr<LVal>>(value_);
    }

    [[nodiscard]] bool isNumber() const {
        return std::holds_alternative<std::shared_ptr<Number>>(value_);
    }
};

// UnaryExp -> PrimaryExp | Ident '(' [Exp { ',' Exp }] ')'  | unaryOp UnaryExp
class UnaryExp final : public Node {
    using call = std::pair<std::string, std::vector<std::shared_ptr<Exp>>>;
    using opExp = std::pair<Token::Type, std::shared_ptr<UnaryExp>>;
    const std::variant<call, opExp, std::shared_ptr<PrimaryExp>> value_;

public:
    explicit UnaryExp(const std::shared_ptr<PrimaryExp> &exp) : value_{exp} {}

    UnaryExp(const Token::Type &type, const std::shared_ptr<UnaryExp> &exp) : value_{opExp{type, exp}} {}

    UnaryExp(const std::string &ident, const std::vector<std::shared_ptr<Exp>> &exp) : value_{call{ident, exp}} {}

    [[nodiscard]] bool isPrimaryExp() const {
        return std::holds_alternative<std::shared_ptr<PrimaryExp>>(value_);
    }

    [[nodiscard]] bool isCall() const {
        return std::holds_alternative<call>(value_);
    }

    [[nodiscard]] bool isOpExp() const {
        return std::holds_alternative<opExp>(value_);
    }
};

// MulExp -> UnaryExp { (* | / | %) UnaryExp}
class MulExp final : public Node {
    const std::vector<std::shared_ptr<UnaryExp>> unaryExps_;
    const std::vector<Token::Type> operators_;

public:
    MulExp(const std::vector<std::shared_ptr<UnaryExp>> &unaryExps,
           const std::vector<Token::Type> &operators) : unaryExps_{unaryExps}, operators_{operators} {
        if (operators_.size() != unaryExps_.size() - 1) {
            throw std::invalid_argument("MulExp: Unexpected number of operators");
        }
    }
};

// AddExp -> MulExp { (+ | -) MulExp }
class AddExp final : public Node {
    const std::vector<std::shared_ptr<MulExp>> mulExps_;
    const std::vector<Token::Type> operators_;

public:
    AddExp(const std::vector<std::shared_ptr<MulExp>> &mulExps,
           const std::vector<Token::Type> &operators) : mulExps_{mulExps}, operators_{operators} {
        if (operators_.size() != mulExps_.size() - 1) {
            throw std::invalid_argument("AddExp: Unexpected number of operators");
        }
    }
};

// RelExp -> AddExp { (> | < | >= | <=) AddExp }
class RelExp final : public Node {
    const std::vector<std::shared_ptr<AddExp>> addExps_;
    const std::vector<Token::Type> operators_;

public:
    RelExp(const std::vector<std::shared_ptr<AddExp>> &addExps,
           const std::vector<Token::Type> &operators) : addExps_{addExps}, operators_{operators} {
        if (operators_.size() != addExps_.size() - 1) {
            throw std::invalid_argument("RelExp: Unexpected number of operators");
        }
    }
};

// EqExp -> RelExp { (== | !=) RelExp }
class EqExp final : public Node {
    const std::vector<std::shared_ptr<MulExp>> relExps_;
    const std::vector<Token::Type> operators_;

public:
    EqExp(const std::vector<std::shared_ptr<MulExp>> &relExps,
          const std::vector<Token::Type> &operators) : relExps_{relExps}, operators_{operators} {
        if (operators_.size() != relExps_.size() - 1) {
            throw std::invalid_argument("EqExp: Unexpected number of operators");
        }
    }
};

// LAndExp -> EqExp { && EqExp }
class LAndExp final : public Node {
    const std::vector<std::shared_ptr<EqExp>> eqExps_;

public:
    explicit LAndExp(const std::vector<std::shared_ptr<EqExp>> &eqExps) : eqExps_{eqExps} {}
};

// LOrExp -> LAndExp { || LAndExp }
class LOrExp final : public Node {
    const std::vector<std::shared_ptr<LAndExp>> lAndExps_;

public:
    explicit LOrExp(const std::vector<std::shared_ptr<LAndExp>> &lAndExps) : lAndExps_{lAndExps} {}
};

// Exp -> AddExp
class Exp final : public Node {
    const std::shared_ptr<AddExp> addExp_;

public:
    explicit Exp(const std::shared_ptr<AddExp> &addExp) : addExp_{addExp} {}
};

// ConstExp -> AddExp
class ConstExp final : public Node {
    const std::shared_ptr<AddExp> addExp_;

public:
    explicit ConstExp(const std::shared_ptr<AddExp> &addExp) : addExp_{addExp} {}
};

// Cond -> LOrExp
class Cond final : public Node {
    const std::shared_ptr<LOrExp> lOrExp_;
};

// Decl -> ConstDecl | VarDecl
class Decl : public Node {};

// Stmt -> LVal '=' Exp ';' | [Exp] ';'  | Block
// | 'if' '( Cond ')' Stmt [ 'else' Stmt ]
// | 'while' '(' Cond ')' Stmt
// | 'break' ';'
// | 'continue' ';'
// | 'return' [Exp] ';'
// | 'putf' '(' StringConst {',' Exp} ')' ';'
class Stmt : public Node {};

// Block -> '{' { (Decl | Stmt) } '}'
class Block final : public Node {
    const std::vector<std::variant<std::shared_ptr<Decl>, std::shared_ptr<Stmt>>> items_;

public:
    explicit Block(const std::vector<std::variant<std::shared_ptr<Decl>, std::shared_ptr<Stmt>>> &items) : items_{
        items
    } {};
};

class AssignStmt final : public Stmt {
    const std::shared_ptr<LVal> lval_;
    const std::shared_ptr<Exp> exp_;

public:
    AssignStmt(const std::shared_ptr<LVal> &lval, const std::shared_ptr<Exp> &exp) : lval_{lval}, exp_{exp} {}
};

class ExpStmt final : public Stmt {
    const std::shared_ptr<Exp> exp_;

public:
    explicit ExpStmt(const std::shared_ptr<Exp> &exp) : exp_{exp} {}
};

class BlockStmt final : public Stmt {
    const std::shared_ptr<Block> block;

public:
    explicit BlockStmt(const std::shared_ptr<Block> &block) : block{block} {};
};

class IfStmt final : public Stmt {
    const std::shared_ptr<Cond> cond_;
    const std::shared_ptr<Stmt> then_;
    const std::shared_ptr<Stmt> else_;

public:
    IfStmt(const std::shared_ptr<Cond> &cond, const std::shared_ptr<Stmt> &then,
           const std::shared_ptr<Stmt> &else_) : cond_{cond}, then_{then}, else_{else_} {}
};

class WhileStmt final : public Stmt {
    const std::shared_ptr<Cond> cond_;
    const std::shared_ptr<Stmt> body_;

public:
    WhileStmt(const std::shared_ptr<Cond> &cond, const std::shared_ptr<Stmt> &body) : cond_{cond}, body_{body} {}
};

class BreakStmt final : public Stmt {};

class ContinueStmt final : public Stmt {};

class ReturnStmt final : public Stmt {
    const std::shared_ptr<Exp> exp_;

public:
    explicit ReturnStmt(const std::shared_ptr<Exp> &exp) : exp_{exp} {}
};

class PutfStmt final : public Stmt {
    const std::string string_const_;
    const std::vector<std::shared_ptr<Exp>> exps_;

public:
    PutfStmt(std::string string_const, const std::vector<std::shared_ptr<Exp>> &exps)
        : string_const_{std::move(string_const)}, exps_{exps} {}
};

// ConstInitVal -> ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
class ConstInitVal final : public Node {
    const std::variant<std::shared_ptr<ConstExp>, std::vector<std::shared_ptr<ConstInitVal>>> value_;

public:
    explicit ConstInitVal(const std::shared_ptr<ConstExp> &constExp) : value_{constExp} {}

    explicit ConstInitVal(const std::vector<std::shared_ptr<ConstInitVal>> &constInitVals) : value_{constInitVals} {}

    [[nodiscard]] bool isConstExp() const {
        return std::holds_alternative<std::shared_ptr<ConstExp>>(value_);
    }

    [[nodiscard]] bool isConstInitVals() const {
        return std::holds_alternative<std::vector<std::shared_ptr<ConstInitVal>>>(value_);
    }
};

// ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
class ConstDef final : public Node {
    const std::string ident_;
    const std::vector<std::shared_ptr<ConstExp>> constExps_;
    const std::shared_ptr<ConstInitVal> constInitVal_;

public:
    ConstDef(std::string ident, const std::vector<std::shared_ptr<ConstExp>> &constExps,
             const std::shared_ptr<ConstInitVal> &constInitVal) :
        ident_{std::move(ident)}, constExps_{constExps}, constInitVal_{constInitVal} {}
};

// ConstDecl ->  'const' BType ConstDef { ',' ConstDef } ';'
class ConstDecl final : public Decl {
    const Token::Type bType_;
    const std::vector<std::shared_ptr<ConstDef>> constDefs_;

public:
    ConstDecl(const Token::Type &bType, const std::vector<std::shared_ptr<ConstDef>> &constDefs) :
        bType_{bType}, constDefs_{constDefs} {}
};

// InitVal : Exp | '{' [ InitVal { ',' InitVal } ] '}'
class InitVal final : public Node {
    const std::variant<std::shared_ptr<Exp>, std::vector<std::shared_ptr<InitVal>>> value_;

public:
    explicit InitVal(const std::shared_ptr<Exp> &exp) : value_{exp} {}

    explicit InitVal(const std::vector<std::shared_ptr<InitVal>> &initVals) : value_{initVals} {}

    [[nodiscard]] bool isExp() const {
        return std::holds_alternative<std::shared_ptr<Exp>>(value_);
    }

    [[nodiscard]] bool isInitVals() const {
        return std::holds_alternative<std::vector<std::shared_ptr<InitVal>>>(value_);
    }
};

// VarDef -> Ident { '[' ConstExp ']' } ('=' InitVal)
class VarDef final : public Node {
    const std::string ident_;
    const std::vector<std::shared_ptr<ConstExp>> constExps_;
    const std::shared_ptr<InitVal> initVal_;

public:
    VarDef(std::string ident, const std::vector<std::shared_ptr<ConstExp>> &constExps,
           const std::shared_ptr<InitVal> &initVal) :
        ident_{std::move(ident)}, constExps_{constExps}, initVal_{initVal} {}
};

// VarDecl -> BType VarDef { ',' VarDef } ';'
class VarDecl final : public Decl {
    const Token::Type bType_;
    const std::vector<std::shared_ptr<VarDef>> varDefs_;

public:
    VarDecl(const Token::Type &bType, const std::vector<std::shared_ptr<VarDef>> &varDefs) :
        bType_{bType}, varDefs_{varDefs} {}
};

// FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
class FuncFParam final : public Node {
    const Token::Type bType_;
    const std::string ident_;
    const std::vector<std::shared_ptr<Exp>> exps_;

public:
    FuncFParam(const Token::Type &bType, std::string ident,
               const std::vector<std::shared_ptr<Exp>> &exps) :
        bType_{bType}, ident_{std::move(ident)}, exps_{exps} {}
};

// FuncDef -> FuncType Ident '(' [FuncFParam { ',' FuncFParam }] ')' Block
class FuncDef final : public Node {
    const Token::Type funcType_;
    const std::string ident_;
    const std::vector<std::shared_ptr<FuncFParam>> funcParams_;
    const std::shared_ptr<Block> block_;

public:
    FuncDef(const Token::Type &funcType, std::string ident,
            const std::vector<std::shared_ptr<FuncFParam>> &funcParams,
            const std::shared_ptr<Block> &block) :
        funcType_{funcType}, ident_{std::move(ident)}, funcParams_{funcParams}, block_{block} {}
};

// CompUnit -> {Decl | FuncDef}
class CompUnit final : public Node {
    const std::vector<std::variant<std::shared_ptr<Decl>, std::shared_ptr<FuncDef>>> compunits_;

public:
    explicit CompUnit(const std::vector<std::variant<std::shared_ptr<Decl>,
        std::shared_ptr<FuncDef>>> &compunits) :
        compunits_{compunits} {};
};
}
#endif
