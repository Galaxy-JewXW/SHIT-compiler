#include "Mir/Builder.h"

#include <algorithm>
#include <unordered_set>

#include "Mir/Instruction.h"
#include "Mir/Structure.h"
#include "Utils/Log.h"

namespace Mir {
size_t Builder::block_count{0}, Builder::variable_count{0};

[[nodiscard]] std::shared_ptr<Module> &Builder::visit(const std::shared_ptr<AST::CompUnit> &ast) {
    for (const auto &unit: ast->compunits()) {
        if (std::holds_alternative<std::shared_ptr<AST::Decl>>(unit)) {
            is_global = true;
            visit_decl(std::get<std::shared_ptr<AST::Decl>>(unit));
            is_global = false;
        } else if (std::holds_alternative<std::shared_ptr<AST::FuncDef>>(unit)) {
            visit_funcDef(std::get<std::shared_ptr<AST::FuncDef>>(unit));
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
    auto ir_type = Type::get_type(type);
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
    std::shared_ptr<Value> address = nullptr;
    if (is_global) {
        const auto &gv = std::make_shared<GlobalVariable>(constDef->ident(), ir_type, true, init_value);
        module->add_global_variable(gv);
        address = gv;
    } else {
        const auto alloc = Alloc::create(gen_variable_name(), ir_type, cur_block);
        address = alloc;
        if (init_value->is_constant_init())
            std::dynamic_pointer_cast<Init::Constant>(init_value)->gen_store_inst(address, cur_block);
        else
            std::dynamic_pointer_cast<Init::Array>(init_value)->gen_store_inst(address, cur_block, dimensions);
    }
    table->insert_symbol(constDef->ident(), ir_type, true, init_value, address);
}

void Builder::visit_varDecl(const std::shared_ptr<AST::VarDecl> &varDecl) {
    for (const auto &varDef: varDecl->varDefs()) {
        visit_varDef(varDecl->bType(), varDef);
    }
}

void Builder::visit_varDef(const Token::Type type, const std::shared_ptr<AST::VarDef> &varDef) {}

void Builder::visit_funcDef(const std::shared_ptr<AST::FuncDef> &funcDef) {
    const auto &ir_type = Type::get_type(funcDef->funcType());
    const auto &ident = funcDef->ident();
    if (table->lookup_in_current_scope(ident)) { log_error("Redefinition of %s", ident.c_str()); }
    // 创建局部符号表
    table->push_scope();
    // 如果存在形参，获取函数形参的种类
    std::vector<std::pair<std::string, std::shared_ptr<Type::Type>>> arguments;
    if (!funcDef->funcParams().empty()) {
        for (const auto &funcFParam: funcDef->funcParams()) {
            arguments.emplace_back(visit_funcFParam(funcFParam));
        }
        auto all_unique = [&] {
            std::unordered_set<std::string> seen;
            return std::all_of(arguments.begin(), arguments.end(),
                               [&seen](const auto &pair) {
                                   return seen.insert(pair.first).second;
                               });
        };
        if (!all_unique()) { log_error("Duplicated argument"); }
    }
    // 创建llvm Function和第一个block，记作entry block
    const auto func = std::make_shared<Function>(ident, ir_type);
    cur_function = func;
    module->add_function(func);
    // 清零变量计数器、基本块计数器
    reset_count();
    // 创建第一个block
    const auto entry_block = Block::create(gen_block_name(), func);
    cur_block = entry_block;
    for (auto i = 0u; i < arguments.size(); ++i) {
        auto &[ident, ir_type] = arguments[i];
        const auto argument = std::make_shared<Argument>(gen_variable_name(), ir_type, i);
        func->add_argument(argument);
    }
    for (auto i = 0u; i < arguments.size(); ++i) {
        auto &[ident, ir_type] = arguments[i];
        const auto addr = Alloc::create(gen_variable_name(), ir_type, cur_block);
        Store::create(addr, func->get_arguments()[i], cur_block);
        table->insert_symbol(ident, ir_type, false, nullptr, addr);
    }
    visit_block(funcDef->block());
    table->pop_scope();
}

std::pair<std::string, std::shared_ptr<Type::Type>>
Builder::visit_funcFParam(const std::shared_ptr<AST::FuncFParam> &funcFParam) const {
    auto ir_type = Type::get_type(funcFParam->bType());
    const auto &ident = funcFParam->ident();
    if (funcFParam->exps().empty()) {
        return {ident, ir_type};
    }
    std::vector<int> dimensions;
    for (const auto &exp: funcFParam->exps()) {
        if (!exp) continue;
        const auto res = eval_exp(exp->addExp(), table);
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
    return {ident, std::make_shared<Type::Pointer>(ir_type)};
}

void Builder::visit_block(const std::shared_ptr<AST::Block> &block) {
    for (const auto &item: block->items()) {
        if (std::holds_alternative<std::shared_ptr<AST::Decl>>(item)) {
            visit_decl(std::get<std::shared_ptr<AST::Decl>>(item));
        } else if (std::holds_alternative<std::shared_ptr<AST::Stmt>>(item)) {
            // visit_stmt(std::get<std::shared_ptr<AST::Stmt>>(item));
        } else {
            log_fatal("Unknown item type");
        }
    }
}
}
