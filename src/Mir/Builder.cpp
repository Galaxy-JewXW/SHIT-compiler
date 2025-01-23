#include "Mir/Builder.h"

#include "Mir/Instruction.h"
#include "Mir/Structure.h"
#include "Utils/Log.h"

namespace Mir {
[[nodiscard]] std::shared_ptr<Module> &Builder::visit(const std::shared_ptr<AST::CompUnit> &ast) {
    for (const auto &unit: ast->compunits()) {
        if (std::holds_alternative<std::shared_ptr<AST::Decl>>(unit)) {
            is_global = true;
            visit_decl(std::get<std::shared_ptr<AST::Decl>>(unit));
            is_global = false;
        } else if (std::holds_alternative<std::shared_ptr<AST::FuncDef>>(unit)) {
            // visit_funcDef(std::get<std::shared_ptr<AST::FuncDef>>(unit));
        }
    }
    return module;
}

void Builder::visit_decl(const std::shared_ptr<AST::Decl> &decl) {
    if (std::dynamic_pointer_cast<AST::ConstDecl>(decl)) {
        visit_constDecl(std::dynamic_pointer_cast<AST::ConstDecl>(decl));
    } else if (std::dynamic_pointer_cast<AST::VarDecl>(decl)) {
        visit_varDecl(std::dynamic_pointer_cast<AST::VarDecl>(decl));
    } else {
        log_fatal("unknown decl type");
    }
}

void Builder::visit_constDecl(const std::shared_ptr<AST::ConstDecl> &constDecl) const {
    for (const auto &constDef: constDecl->constDefs()) {
        visit_constDef(constDecl->bType(), constDef);
    }
}

void Builder::visit_constDef(const Token::Type type, const std::shared_ptr<AST::ConstDef> &constDef) const {
    std::shared_ptr<Type::Type> ir_type = Type::get_type(type);
    if (ir_type->is_void()) { log_error("Cannot define a void variable"); }
    std::vector<int> dimensions;
    for (const auto &constExp: constDef->constExps()) {
        const auto res = eval_exp(constExp->addExp(), table);
        int dim;
        if (std::holds_alternative<int>(res)) {
            dim = std::get<int>(res);
        } else if (std::holds_alternative<float>(res)) {
            dim = static_cast<int>(std::get<float>(res));
        } else {
            log_error("Unexpected error on calculating dim");
        }
        if (dim <= 0) { log_error("Non-Positive dimension: %d", dim); }
        dimensions.push_back(dim);
    }
    for (auto it = dimensions.rbegin(); it != dimensions.rend(); ++it) {
        const auto dim = *it;
        ir_type = std::make_shared<Type::Array>(dim, ir_type);
    }
    // ConstDef必然含有ConstInitVal
    std::shared_ptr<Init::Init> init_value;
    const auto &constInitVal = constDef->constInitVal();
    if (ir_type->is_int32() || ir_type->is_float()) {
        if (constInitVal->is_constInitVals()) { log_fatal("Variable cannot be initialized as an array"); }
        const auto constExp = std::get<std::shared_ptr<AST::ConstExp>>(constInitVal->get_value());
        init_value = Init::Constant::create_constant_init_value(ir_type, constExp->addExp(), table);
    } else if (ir_type->is_array()) {
        if (constInitVal->is_constExp()) { log_fatal("Array cannot be initialized as a scalar"); }
        init_value = Init::Array::create_array_init_value<AST::ConstInitVal>(ir_type, constInitVal, table, true);
    } else {
        log_fatal("Unknown type %s", ir_type->to_string().c_str());
    }
    log_trace("visit_constDef: %s", init_value->to_string().c_str());
}

void Builder::visit_varDecl(const std::shared_ptr<AST::VarDecl> &varDecl) {}
}
