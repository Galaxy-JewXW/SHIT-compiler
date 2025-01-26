#include "Mir/Builder.h"
#include "Mir/Init.h"

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

void Builder::visit_decl(const std::shared_ptr<AST::Decl> &decl) const {
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

void Builder::visit_varDecl(const std::shared_ptr<AST::VarDecl> &varDecl) const {
    for (const auto &varDef: varDecl->varDefs()) {
        visit_varDef(varDecl->bType(), varDef);
    }
}

void Builder::visit_varDef(const Token::Type type, const std::shared_ptr<AST::VarDef> &varDef) const {
    auto ir_type = Type::get_type(type);
    if (ir_type->is_void()) { log_error("Cannot define a void variable"); }
    std::vector<int> dimensions;
    for (const auto &constExp: varDef->constExps()) {
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
    std::shared_ptr<Init::Init> init_value;
    if (const auto &initVal = varDef->initVal()) {
        if (ir_type->is_int32() || ir_type->is_float()) {
            if (initVal->is_initVals()) { log_fatal("Variable cannot be initialized as an array"); }
            const auto &exp = std::get<std::shared_ptr<AST::Exp>>(initVal->get_value());
            if (is_global) {
                init_value = Init::Constant::create_constant_init_value(ir_type, exp->addExp(), table);
            } else {
                const auto &value = visit_exp(exp);
                init_value = Init::Exp::create_exp_init_value(ir_type, type_cast(value, ir_type, cur_block));
            }
        } else if (ir_type->is_array()) {
            if (initVal->is_exp()) { log_fatal("Array cannot be initialized as a scalar"); }
            init_value = Init::Array::create_array_init_value<AST::InitVal>(ir_type, initVal, table, is_global, this);
        } else {
            log_error("Invalid type %s", ir_type->to_string().c_str());
        }
    } else {
        if (ir_type->is_int32() || ir_type->is_float()) {
            init_value = Init::Constant::create_zero_constant_init_value(ir_type);
        } else if (ir_type->is_array()) {
            init_value = Init::Array::create_zero_array_init_value(ir_type);
        } else {
            log_error("Invalid type: %s", ir_type->to_string().c_str());
        }
    }
    std::shared_ptr<Value> address = nullptr;
    if (is_global) {
        const auto &gv = std::make_shared<GlobalVariable>(varDef->ident(), ir_type, false, init_value);
        module->add_global_variable(gv);
        address = gv;
    } else {
        const auto alloc = Alloc::create(gen_variable_name(), ir_type, cur_block);
        address = alloc;
        if (init_value->is_constant_init())
            std::dynamic_pointer_cast<Init::Constant>(init_value)->gen_store_inst(address, cur_block);
        else if (init_value->is_array_init())
            std::dynamic_pointer_cast<Init::Array>(init_value)->gen_store_inst(address, cur_block, dimensions);
        else
            std::dynamic_pointer_cast<Init::Exp>(init_value)->gen_store_inst(address, cur_block);
    }
    table->insert_symbol(varDef->ident(), ir_type, false, init_value, address);
}

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

void Builder::visit_block(const std::shared_ptr<AST::Block> &block) const {
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

std::shared_ptr<Value> Builder::visit_exp(const std::shared_ptr<AST::Exp> &exp) const {
    return visit_addExp(exp->addExp());
}

std::shared_ptr<Value> Builder::visit_addExp(const std::shared_ptr<AST::AddExp> &addExp) const {
    const auto &mul_exps = addExp->mulExps();
    auto lhs = visit_mulExp(mul_exps[0]);
    for (size_t i = 1; i < mul_exps.size(); ++i) {
        const auto &rhs = visit_mulExp(mul_exps[i]);
        if (const auto op = addExp->operators()[i - 1]; op == Token::Type::ADD) {
            if (lhs->get_type()->is_float() || rhs->get_type()->is_float()) {
                lhs = FAdd::create(gen_variable_name(), type_cast(lhs, Type::Float::f32, cur_block),
                                   type_cast(rhs, Type::Float::f32, cur_block), cur_block);
            } else {
                lhs = Add::create(gen_variable_name(), type_cast(lhs, Type::Integer::i32, cur_block),
                                  type_cast(rhs, Type::Integer::i32, cur_block), cur_block);
            }
        } else if (op == Token::Type::SUB) {
            if (lhs->get_type()->is_float() || rhs->get_type()->is_float()) {
                lhs = FSub::create(gen_variable_name(), type_cast(lhs, Type::Float::f32, cur_block),
                                   type_cast(rhs, Type::Float::f32, cur_block), cur_block);
            } else {
                lhs = Sub::create(gen_variable_name(), type_cast(lhs, Type::Integer::i32, cur_block),
                                  type_cast(rhs, Type::Integer::i32, cur_block), cur_block);
            }
        }
    }
    return lhs;
}

std::shared_ptr<Value> Builder::visit_mulExp(const std::shared_ptr<AST::MulExp> &mulExp) const {
    const auto &unary_exps = mulExp->unaryExps();
    auto lhs = visit_unaryExp(unary_exps[0]);
    for (size_t i = 1; i < unary_exps.size(); ++i) {
        const auto &rhs = visit_unaryExp(unary_exps[i]);
        if (const auto op = mulExp->operators()[i - 1]; op == Token::Type::MUL) {
            if (lhs->get_type()->is_float() || rhs->get_type()->is_float()) {
                lhs = FMul::create(gen_variable_name(), type_cast(lhs, Type::Float::f32, cur_block),
                                   type_cast(rhs, Type::Float::f32, cur_block), cur_block);
            } else {
                lhs = Mul::create(gen_variable_name(), type_cast(lhs, Type::Integer::i32, cur_block),
                                  type_cast(rhs, Type::Integer::i32, cur_block), cur_block);
            }
        } else if (op == Token::Type::DIV) {
            if (lhs->get_type()->is_float() || rhs->get_type()->is_float()) {
                lhs = FDiv::create(gen_variable_name(), type_cast(lhs, Type::Float::f32, cur_block),
                                   type_cast(rhs, Type::Float::f32, cur_block), cur_block);
            } else {
                lhs = Div::create(gen_variable_name(), type_cast(lhs, Type::Integer::i32, cur_block),
                                  type_cast(rhs, Type::Integer::i32, cur_block), cur_block);
            }
        } else if (op == Token::Type::MOD) {
            if (lhs->get_type()->is_float() || rhs->get_type()->is_float()) {
                lhs = FMod::create(gen_variable_name(), type_cast(lhs, Type::Float::f32, cur_block),
                                   type_cast(rhs, Type::Float::f32, cur_block), cur_block);
            } else {
                lhs = Mod::create(gen_variable_name(), type_cast(lhs, Type::Integer::i32, cur_block),
                                  type_cast(rhs, Type::Integer::i32, cur_block), cur_block);
            }
        }
    }
    return lhs;
}

std::shared_ptr<Value> visit_functionCall(const AST::UnaryExp::call &call) {
    // TODO
    log_error("Unimplemented function call");
}

std::shared_ptr<Value> Builder::visit_unaryExp(const std::shared_ptr<AST::UnaryExp> &unaryExp) const {
    if (unaryExp->is_primaryExp()) {
        return visit_primaryExp(std::get<std::shared_ptr<AST::PrimaryExp>>(unaryExp->get_value()));
    }
    if (unaryExp->is_call()) {
        return visit_functionCall(std::get<AST::UnaryExp::call>(unaryExp->get_value()));
    }
    if (unaryExp->is_opExp()) {
        const auto &[type, sub_exp] = std::get<AST::UnaryExp::opExp>(unaryExp->get_value());
        const auto &sub_exp_value = visit_unaryExp(sub_exp);
        if (type == Token::Type::ADD) {
            return type_cast(sub_exp_value, Type::Integer::i32, cur_block);
        }
        if (type == Token::Type::SUB) {
            if (sub_exp_value->get_type()->is_float()) {
                return FSub::create(gen_variable_name(), std::make_shared<ConstFloat>(0.0f),
                                    type_cast(sub_exp_value, Type::Float::f32, cur_block), cur_block);
            }
            return Sub::create(gen_variable_name(), std::make_shared<ConstInt>(0),
                               type_cast(sub_exp_value, Type::Integer::i32, cur_block), cur_block);
        }
        if (type == Token::Type::NOT) {
            if (sub_exp_value->get_type()->is_float()) {
                const auto &cmp = Fcmp::create(gen_variable_name(), Fcmp::Op::EQ,
                                               type_cast(sub_exp_value, Type::Float::f32, cur_block),
                                               std::make_shared<ConstFloat>(0.0f), cur_block);
                return type_cast(cmp, Type::Integer::i32, cur_block);
            }
            const auto &cmp = Icmp::create(gen_variable_name(), Icmp::Op::EQ,
                                           type_cast(sub_exp_value, Type::Integer::i32, cur_block),
                                           std::make_shared<ConstInt>(0), cur_block);
            return type_cast(cmp, Type::Integer::i32, cur_block);
        }
    }
    log_fatal("Invalid unaryExp");
}

std::shared_ptr<Value> Builder::visit_primaryExp(const std::shared_ptr<AST::PrimaryExp> &primaryExp) const {
    if (primaryExp->is_number()) {
        return visit_number(std::get<std::shared_ptr<AST::Number>>(primaryExp->get_value()));
    }
    if (primaryExp->is_lVal()) {
        return visit_lVal(std::get<std::shared_ptr<AST::LVal>>(primaryExp->get_value()));
    }
    if (primaryExp->is_exp()) {
        return visit_exp(std::get<std::shared_ptr<AST::Exp>>(primaryExp->get_value()));
    }
    log_fatal("Invalid primaryExp");
}

std::shared_ptr<Value> Builder::visit_number(const std::shared_ptr<AST::Number> &number) {
    if (const auto &num = std::dynamic_pointer_cast<AST::FloatNumber>(number)) {
        return std::make_shared<ConstFloat>(num->get_value());
    }
    if (const auto &num = std::dynamic_pointer_cast<AST::IntNumber>(number)) {
        return std::make_shared<ConstInt>(num->get_value());
    }
    log_fatal("Invalid number");
}

std::shared_ptr<Value> Builder::visit_lVal(const std::shared_ptr<AST::LVal> &lVal, const bool get_address) const {
    const auto &ident = lVal->ident();
    const auto &symbol = table->lookup_in_all_scopes(ident);
    if (!symbol) {
        log_error("Undefined variable: %s", ident.c_str());
    }
    const auto &address = symbol->get_address();
    if (!address->get_type()->is_pointer()) {
        log_fatal("Invalid address type %s of %s", address->get_type()->to_string().c_str(), ident.c_str());
    }
    const auto &ir_type = std::dynamic_pointer_cast<Type::Pointer>(address->get_type())->get_contain_type();
    if ((ir_type->is_integer() || ir_type->is_float()) && lVal->exps().empty()) {
        return Load::create(gen_variable_name(), address, cur_block);
    }
    std::shared_ptr<Value> pointer = address;
    auto content_type = ir_type;
    std::vector<std::shared_ptr<Value>> indexes;
    for (const auto &exp: lVal->exps()) {
        const auto &idx_value = type_cast(visit_exp(exp), Type::Integer::i32, cur_block);
        if (content_type->is_pointer()) {
            content_type = std::dynamic_pointer_cast<Type::Pointer>(content_type)->get_contain_type();
        } else if (content_type->is_array()) {
            content_type = std::dynamic_pointer_cast<Type::Array>(content_type)->get_element_type();
        } else {
            log_error("Invalid content type %s", content_type->to_string().c_str());
        }
        indexes.emplace_back(idx_value);
    }
    for (const auto &idx: indexes) {
        pointer = GetElementPtr::create(gen_variable_name(), pointer, idx, cur_block);
    }
    if (get_address) { return pointer; }
    if (content_type->is_integer() || content_type->is_float()) {
        return Load::create(gen_variable_name(), pointer, cur_block);
    }
    if (content_type->is_array()) {
        return GetElementPtr::create(gen_variable_name(), pointer, std::make_shared<ConstInt>(0), cur_block);
    }
    log_fatal("Invalid lVal");
}
}
