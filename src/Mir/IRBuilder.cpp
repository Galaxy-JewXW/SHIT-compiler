#include "Mir/IRBuilder.h"
#include "Mir/IRStructure.h"
#include "Mir/Value.h"

std::shared_ptr<IRModule> IRBuilder::visitAST(const std::shared_ptr<AST::CompUnit> &compUnit) {
    auto* irModule = new IRModule();
    std::vector<std::shared_ptr<IRGlobalVariable>> globalVariables;
    std::vector<std::shared_ptr<IRFunction>> functions;
    for (const auto& element : compUnit->compunits()) {
        if (std::holds_alternative<std::shared_ptr<AST::Decl>>(element)) {
            auto decl = std::get<std::shared_ptr<AST::Decl>>(element);
            for (const std::shared_ptr<IRGlobalVariable>& ele : visitGlobalDecl(decl)) {
                irModule->addGlobalVariable(ele);
            }
        }
        else if (std::holds_alternative<std::shared_ptr<AST::FuncDef>>(element)) {
            auto func = std::get<std::shared_ptr<AST::FuncDef>>(element);
            irModule->addFunction(visitFunction(func));
        }
    }
    return std::shared_ptr<IRModule>(irModule);
}

std::vector<std::shared_ptr<IRGlobalVariable> > IRBuilder::visitGlobalDecl(const std::shared_ptr<AST::Decl> & decl) {
    std::vector<std::shared_ptr<IRGlobalVariable>> globalVariables;
    const AST::Decl& _decl = *decl;
    if (const auto constDecl = dynamic_cast<const AST::ConstDecl*> (&_decl)) {
        const Token::Type type = constDecl->bType();
        for (const std::shared_ptr<AST::ConstDef>& constDef : constDecl->constDefs()) {
            globalVariables.push_back(visitGlobalConstDef(type, constDef));
        }
    }
    else if (const auto varDecl = dynamic_cast<const AST::VarDecl*> (&_decl)) {
        const Token::Type type = varDecl->bType();
        for (const std::shared_ptr<AST::VarDef>& varDef : varDecl->varDefs()) {
            globalVariables.push_back(visitGlobalVarDef(type, varDef));
        }
    }
    return globalVariables;
}

std::shared_ptr<IRGlobalVariable> IRBuilder::visitGlobalConstDef(Token::Type type, const std::shared_ptr<AST::ConstDef>& constDef) {
    IRTYPE::IRType irType = type == (Token::Type::INT) ? IRTYPE::IRIntegerType::getIntegerType() :
                                                         IRTYPE::IRFloatType::getFloatType();
    std::string name = constDef.ident();
    std::shared_ptr<AST::ConstInitVal> init = constDef.constInitVal();
    if (constDef.is_exp()) {

    }
    else {

    }
 }

std::shared_ptr<IRGlobalVariable> IRBuilder::visitGlobalVarDef(Token::Type type,const std::shared_ptr<AST::VarDef>& varDef) {}


std::shared_ptr<IRFunction> IRBuilder::visitFunction(const std::shared_ptr<AST::FuncDef>& funcDef) {
    std::string name = funcDef->ident();
    const Token::Type type = funcDef->funcType();
    std::shared_ptr<IRTYPE::IRType> irType = type == (Token::Type::INT) ? IRTYPE::IRIntegerType::getIntegerType() :
                                            type == (Token::Type::FLOAT) ? IRTYPE::IRFloatType::getFloatType() :
                                            IRTYPE::IRVoidType::getVoidType();
    const auto function = std::make_shared<IRFunction>(name, irType);
    this->symbolTable->addSymbol(name, std::static_pointer_cast<Value>(function));

    curBlock = std::make_shared<IRBlock>();
    this->symbolTablePush();
    if ()
}

void IRBuilder::symbolTablePush() {
    symbolTable = std::make_shared<SymbolTable>(symbolTable);
}

