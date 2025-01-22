#ifndef IRBUILDER_H
#define IRBUILDER_H
#include <Utils/AST.h>

#include "IRStructure.h"
#include "Symbol.h"

class IRBuilder {
    std::shared_ptr<SymbolTable> symbolTable;
    std::shared_ptr<IRBlock> curBlock;
    std::vector<std::shared_ptr<IRBlock>> blocks;
    public:
    IRBuilder() : symbolTable(std::make_shared<SymbolTable>(nullptr)) {}

    std::shared_ptr<IRModule> visitAST(const std::shared_ptr<AST::CompUnit>&);
    std::shared_ptr<IRFunction> visitFunction(const std::shared_ptr<AST::FuncDef>&);
    std::vector<std::shared_ptr<IRGlobalVariable>> visitGlobalDecl(const std::shared_ptr<AST::Decl>&);
    std::shared_ptr<IRGlobalVariable> visitGlobalConstDef(Token::Type, const std::shared_ptr<AST::ConstDef>&);
    std::shared_ptr<IRGlobalVariable> visitGlobalVarDef(Token::Type, const std::shared_ptr<AST::VarDef>&);

    std::shared_ptr<IRFunction> visitFunction(const std::shared_ptr<AST::FuncDef> &funcDef);

    void symbolTablePush();
};
#endif //IRBUILDER_H
