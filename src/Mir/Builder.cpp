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
            const auto exp = std::get<std::shared_ptr<AST::Exp>>(initVal->get_value());
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
    for (size_t i = 0; i < arguments.size(); ++i) {
        auto &[ident, ir_type] = arguments[i];
        const auto argument = std::make_shared<Argument>(gen_variable_name(), ir_type, i);
        func->add_argument(argument);
    }
    for (size_t i = 0; i < arguments.size(); ++i) {
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
            visit_stmt(std::get<std::shared_ptr<AST::Stmt>>(item));
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

std::shared_ptr<Value> Builder::visit_functionCall(const AST::UnaryExp::call &call) const {
    const auto &[ident, args] = call;
    auto func = module->get_function(ident.content);
    if (!func) {
        if (const auto it = Function::runtime_functions.find(ident.content);
            it != Function::runtime_functions.end()) {
            func = it->second;
            module->add_used_runtime_functions(func);
        } else {
            log_error("Unknown function: %s", ident.content.c_str());
        }
    }
    // 实参列表
    std::vector<std::shared_ptr<Value>> r_params;
    if (ident.content == "starttime" || ident.content == "stoptime") {
        r_params.emplace_back(std::make_shared<ConstInt>(ident.line));
        return Call::create(func, r_params, cur_block);
    }
    if (ident.content == "putf") {
        if (!args[0]->is_const_string()) { log_fatal("First parameter of putf must be a const string"); }
        const auto &const_string = args[0]->get_const_string();
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i]->is_const_string()) { log_fatal("Parameter should not be a const string"); }
            r_params.emplace_back(visit_exp(args[i]));
        }
        module->add_const_string(const_string);
        return Call::create(func, r_params, cur_block, static_cast<int>(module->get_const_string_size()));
    }
    const auto &arguments = func->get_arguments();
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i]->is_const_string()) { log_fatal("Parameter should not be a const string"); }
        if (const auto f_type = arguments[i]->get_type(); f_type->is_int32() || f_type->is_float()) {
            r_params.emplace_back(type_cast(visit_exp(args[i]), f_type, cur_block));
        } else {
            const auto p = visit_exp(args[i]);
            r_params.emplace_back(p);
        }
    }
    if (func->get_return_type()->is_void()) {
        return Call::create(func, r_params, cur_block);
    }
    return Call::create(gen_variable_name(), func, r_params, cur_block);
}

std::shared_ptr<Value> Builder::visit_unaryExp(const std::shared_ptr<AST::UnaryExp> &unaryExp) const {
    if (unaryExp->is_primaryExp()) {
        return visit_primaryExp(std::get<std::shared_ptr<AST::PrimaryExp>>(unaryExp->get_value()));
    }
    if (unaryExp->is_call()) {
        return visit_functionCall(std::get<AST::UnaryExp::call>(unaryExp->get_value()));
    }
    if (unaryExp->is_opExp()) {
        const auto [type, sub_exp] = std::get<AST::UnaryExp::opExp>(unaryExp->get_value());
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
    if ((ir_type->is_integer() || ir_type->is_float()) && lVal->exps().empty() && !get_address) {
        return Load::create(gen_variable_name(), address, cur_block);
    }
    std::shared_ptr<Value> pointer = address;
    auto content_type = ir_type;
    std::vector<std::shared_ptr<Value>> indexes;
    for (const auto &exp: lVal->exps()) {
        const auto &idx_value = type_cast(visit_exp(exp), Type::Integer::i32, cur_block);
        if (content_type->is_pointer()) {
            pointer = Load::create(gen_variable_name(), pointer, cur_block);
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

void Builder::visit_cond(const std::shared_ptr<AST::Cond> &cond, const std::shared_ptr<Block> &_then,
                         const std::shared_ptr<Block> &_else) {
    visit_lOrExp(cond->lOrExp(), _then, _else);
}

void Builder::visit_lOrExp(const std::shared_ptr<AST::LOrExp> &lOrExp, const std::shared_ptr<Block> &_then,
                           const std::shared_ptr<Block> &_else) {
    const auto &lAndExps = lOrExp->lAndExps();
    for (size_t i = 0; i < lAndExps.size(); ++i) {
        const auto lAndExp = lAndExps[i];
        if (i == lAndExps.size() - 1) {
            visit_lAndExp(lAndExp, _then, _else);
        } else {
            const auto next = Block::create(gen_block_name(), cur_function);
            visit_lAndExp(lAndExp, _then, next);
            cur_block = next;
        }
    }
}

void Builder::visit_lAndExp(const std::shared_ptr<AST::LAndExp> &lAndExp, const std::shared_ptr<Block> &_then,
                            const std::shared_ptr<Block> &_else) {
    const auto &eqExps = lAndExp->eqExps();
    for (size_t i = 0; i < eqExps.size(); ++i) {
        const auto eqExp = eqExps[i];
        if (i == eqExps.size() - 1) {
            const auto eq = type_cast(visit_eqExp(eqExp), Type::Integer::i1, cur_block);
            Branch::create(eq, _then, _else, cur_block);
        } else {
            const auto next = Block::create(gen_block_name(), cur_function);
            const auto eq = type_cast(visit_eqExp(eqExp), Type::Integer::i1, cur_block);
            Branch::create(eq, next, _else, cur_block);
            cur_block = next;
        }
    }
}

std::shared_ptr<Value> Builder::visit_eqExp(const std::shared_ptr<AST::EqExp> &eqExp) const {
    const auto &relExps = eqExp->relExps();
    auto lhs = visit_relExp(relExps[0]);
    for (size_t i = 1; i < relExps.size(); ++i) {
        if (auto rhs = visit_relExp(relExps[i]); lhs->get_type()->is_float() || rhs->get_type()->is_float()) {
            if (const auto op = eqExp->operators()[i - 1]; op == Token::Type::EQ) {
                lhs = Fcmp::create(gen_variable_name(), Fcmp::Op::EQ,
                                   type_cast(lhs, Type::Float::f32, cur_block),
                                   type_cast(rhs, Type::Float::f32, cur_block), cur_block);
            } else if (op == Token::Type::NE) {
                lhs = Fcmp::create(gen_variable_name(), Fcmp::Op::NE,
                                   type_cast(lhs, Type::Float::f32, cur_block),
                                   type_cast(rhs, Type::Float::f32, cur_block), cur_block);
            } else {
                log_error("Invalid relExp operator %s", Token::type_to_string(op).c_str());
            }
        } else {
            if (const auto op = eqExp->operators()[i - 1]; op == Token::Type::EQ) {
                lhs = Icmp::create(gen_variable_name(), Icmp::Op::EQ,
                                   type_cast(lhs, Type::Integer::i32, cur_block),
                                   type_cast(rhs, Type::Integer::i32, cur_block), cur_block);
            } else if (op == Token::Type::NE) {
                lhs = Icmp::create(gen_variable_name(), Icmp::Op::NE,
                                   type_cast(lhs, Type::Integer::i32, cur_block),
                                   type_cast(rhs, Type::Integer::i32, cur_block), cur_block);
            } else {
                log_error("Invalid relExp operator %s", Token::type_to_string(op).c_str());
            }
        }
    }
    if (!lhs->get_type()->is_integer() && !lhs->get_type()->is_float()) {
        log_error("Invalid relExp ", lhs->to_string().c_str());
    }
    return lhs;
}

std::shared_ptr<Value> Builder::visit_relExp(const std::shared_ptr<AST::RelExp> &relExp) const {
    const auto &addExps = relExp->addExps();
    auto lhs = visit_addExp(addExps[0]);
    for (size_t i = 1; i < addExps.size(); ++i) {
        if (auto rhs = visit_addExp(addExps[i]);
            lhs->get_type()->is_float() || rhs->get_type()->is_float()) {
            if (const auto op = relExp->operators()[i - 1]; op == Token::Type::LT) {
                lhs = Fcmp::create(gen_variable_name(), Fcmp::Op::LT,
                                   type_cast(lhs, Type::Float::f32, cur_block),
                                   type_cast(rhs, Type::Float::f32, cur_block), cur_block);
            } else if (op == Token::Type::GT) {
                lhs = Fcmp::create(gen_variable_name(), Fcmp::Op::GT,
                                   type_cast(lhs, Type::Float::f32, cur_block),
                                   type_cast(rhs, Type::Float::f32, cur_block), cur_block);
            } else if (op == Token::Type::LE) {
                lhs = Fcmp::create(gen_variable_name(), Fcmp::Op::LE,
                                   type_cast(lhs, Type::Float::f32, cur_block),
                                   type_cast(rhs, Type::Float::f32, cur_block), cur_block);
            } else if (op == Token::Type::GE) {
                lhs = Fcmp::create(gen_variable_name(), Fcmp::Op::GE,
                                   type_cast(lhs, Type::Float::f32, cur_block),
                                   type_cast(rhs, Type::Float::f32, cur_block), cur_block);
            }
        } else {
            if (const auto op = relExp->operators()[i - 1]; op == Token::Type::LT) {
                lhs = Icmp::create(gen_variable_name(), Icmp::Op::LT,
                                   type_cast(lhs, Type::Integer::i32, cur_block),
                                   type_cast(rhs, Type::Integer::i32, cur_block), cur_block);
            } else if (op == Token::Type::GT) {
                lhs = Icmp::create(gen_variable_name(), Icmp::Op::GT,
                                   type_cast(lhs, Type::Integer::i32, cur_block),
                                   type_cast(rhs, Type::Integer::i32, cur_block), cur_block);
            } else if (op == Token::Type::LE) {
                lhs = Icmp::create(gen_variable_name(), Icmp::Op::LE,
                                   type_cast(lhs, Type::Integer::i32, cur_block),
                                   type_cast(rhs, Type::Integer::i32, cur_block), cur_block);
            } else if (op == Token::Type::GE) {
                lhs = Icmp::create(gen_variable_name(), Icmp::Op::GE,
                                   type_cast(lhs, Type::Integer::i32, cur_block),
                                   type_cast(rhs, Type::Integer::i32, cur_block), cur_block);
            }
        }
    }
    if (!lhs->get_type()->is_integer() && !lhs->get_type()->is_float()) {
        log_error("Invalid relExp ", lhs->to_string().c_str());
    }
    return lhs;
}

void Builder::visit_stmt(const std::shared_ptr<AST::Stmt> &stmt) {
    if (std::dynamic_pointer_cast<AST::BlockStmt>(stmt)) {
        visit_blockStmt(std::dynamic_pointer_cast<AST::BlockStmt>(stmt));
    } else if (std::dynamic_pointer_cast<AST::AssignStmt>(stmt)) {
        visit_assignStmt(std::dynamic_pointer_cast<AST::AssignStmt>(stmt));
    } else if (std::dynamic_pointer_cast<AST::ExpStmt>(stmt)) {
        visit_expStmt(std::dynamic_pointer_cast<AST::ExpStmt>(stmt));
    } else if (std::dynamic_pointer_cast<AST::ReturnStmt>(stmt)) {
        visit_returnStmt(std::dynamic_pointer_cast<AST::ReturnStmt>(stmt));
    } else if (std::dynamic_pointer_cast<AST::IfStmt>(stmt)) {
        visit_ifStmt(std::dynamic_pointer_cast<AST::IfStmt>(stmt));
    } else if (std::dynamic_pointer_cast<AST::WhileStmt>(stmt)) {
        visit_whileStmt(std::dynamic_pointer_cast<AST::WhileStmt>(stmt));
    } else if (std::dynamic_pointer_cast<AST::BreakStmt>(stmt)) {
        visit_breakStmt();
    } else if (std::dynamic_pointer_cast<AST::ContinueStmt>(stmt)) {
        visit_continueStmt();
    } else {
        log_fatal("Invalid stmt type");
    }
}

void Builder::visit_blockStmt(const std::shared_ptr<AST::BlockStmt> &blockStmt) {
    table->push_scope();
    visit_block(blockStmt->block());
    table->pop_scope();
}


void Builder::visit_assignStmt(const std::shared_ptr<AST::AssignStmt> &assignStmt) const {
    const auto symbol = table->lookup_in_all_scopes(assignStmt->lVal()->ident());
    if (!symbol) { log_error("Undefined variable: %s", assignStmt->lVal()->ident().c_str()); }
    if (symbol->is_constant_symbol()) {
        log_error("Cannot assign to constant variable: %s", assignStmt->lVal()->ident().c_str());
    }
    const auto address = visit_lVal(assignStmt->lVal(), true);
    if (!address->get_type()->is_pointer()) {
        log_error("Invalid address type %s", address->get_type()->to_string().c_str());
    }
    const auto ir_type = std::dynamic_pointer_cast<Type::Pointer>(address->get_type())->get_contain_type();
    if (!ir_type->is_int32() && !ir_type->is_float()) {
        log_error("Invalid element type %s", ir_type->to_string().c_str());
    }
    const auto exp_value = visit_exp(assignStmt->exp());
    Store::create(address, type_cast(exp_value, ir_type, cur_block), cur_block);
}

void Builder::visit_expStmt(const std::shared_ptr<AST::ExpStmt> &expStmt) const {
    if (const auto exp = expStmt->exp()) {
        // ReSharper disable once CppExpressionWithoutSideEffects
        visit_exp(exp);
    }
}

void Builder::visit_returnStmt(const std::shared_ptr<AST::ReturnStmt> &returnStmt) const {
    if (const auto exp = returnStmt->exp()) {
        const auto exp_value = visit_exp(exp);
        Ret::create(type_cast(exp_value, cur_function->get_return_type(), cur_block), cur_block);
    } else {
        Ret::create(cur_block);
    }
}

void Builder::visit_ifStmt(const std::shared_ptr<AST::IfStmt> &ifStmt) {
    const auto then_block = Block::create(gen_block_name(), cur_function);
    if (const auto else_stmt = ifStmt->_else()) {
        const auto else_block = Block::create(gen_block_name(), cur_function);
        const auto follow_block = Block::create(gen_block_name(), cur_function);
        visit_cond(ifStmt->cond(), then_block, else_block);
        cur_block = then_block;
        visit_stmt(ifStmt->then());
        Jump::create(follow_block, cur_block);
        cur_block = else_block;
        visit_stmt(else_stmt);
        Jump::create(follow_block, cur_block);
        cur_block = follow_block;
    } else {
        const auto follow_block = Block::create(gen_block_name(), cur_function);
        visit_cond(ifStmt->cond(), then_block, follow_block);
        cur_block = then_block;
        visit_stmt(ifStmt->then());
        Jump::create(follow_block, cur_block);
        cur_block = follow_block;
    }
}

void Builder::visit_whileStmt(const std::shared_ptr<AST::WhileStmt> &whileStmt) {
    auto cond_block = Block::create(gen_block_name(), cur_function),
         body_block = Block::create(gen_block_name(), cur_function),
         follow_block = Block::create(gen_block_name(), cur_function);
    loop_stats.emplace_back(cond_block, body_block, follow_block);
    Jump::create(cond_block, cur_block);
    cur_block = cond_block;
    visit_cond(whileStmt->cond(), body_block, follow_block);
    cur_block = body_block;
    visit_stmt(whileStmt->body());
    visit_cond(whileStmt->cond(), body_block, follow_block);
    cur_block = follow_block;
}

void Builder::visit_breakStmt() {
    auto [cond_block, body_block, follow_block] = loop_stats.back();
    Jump::create(follow_block, cur_block);
}

void Builder::visit_continueStmt() {
    auto [cond_block, body_block, follow_block] = loop_stats.back();
    Jump::create(body_block, cur_block);
}
}
