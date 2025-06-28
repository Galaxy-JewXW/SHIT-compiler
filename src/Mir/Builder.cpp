#include "Mir/Builder.h"
#include "Mir/Const.h"
#include "Mir/Init.h"
#include "Mir/Instruction.h"
#include "Mir/Structure.h"
#include "Utils/Log.h"

#include <algorithm>
#include <cassert>
#include <unordered_set>

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
    // 释放不必要的引用计数
    cur_block = nullptr;
    cur_function = nullptr;
    table = nullptr;
    loop_stats.clear();
    assert(cond_stats.empty());
    assert(loop_stats.empty());
    return module;
}

void Builder::visit_decl(const std::shared_ptr<AST::Decl> &decl) const {
    if (const auto &constDecl = std::dynamic_pointer_cast<AST::ConstDecl>(decl)) {
        visit_constDecl(constDecl);
    } else if (const auto &varDecl = std::dynamic_pointer_cast<AST::VarDecl>(decl)) {
        visit_varDecl(varDecl);
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
        int dim = res.get<int>();
        if (dim <= 0) { log_error("Non-Positive dimension: %d", dim); }
        dimensions.push_back(dim);
    }
    for (auto it = dimensions.rbegin(); it != dimensions.rend(); ++it) {
        const auto dim = *it;
        ir_type = Type::Array::create(dim, ir_type);
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
        if (init_value->is_constant_init()) {
            std::static_pointer_cast<Init::Constant>(init_value)->gen_store_inst(address, cur_block);
        } else {
            std::static_pointer_cast<Init::Array>(init_value)->gen_store_inst(address, cur_block, dimensions);
        }
    }
    table->insert_symbol(constDef->ident(), ir_type, init_value, address, true, false);
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
        int dim = res.get<int>();
        if (dim <= 0) { log_error("Non-Positive dimension: %d", dim); }
        dimensions.push_back(dim);
    }
    for (auto it = dimensions.rbegin(); it != dimensions.rend(); ++it) {
        const auto dim = *it;
        ir_type = Type::Array::create(dim, ir_type);
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
                const auto &casted_value = type_cast(value, ir_type, cur_block);
                init_value = Init::Exp::create_exp_init_value(ir_type, casted_value);
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
        // 在控制流不明确的情况下，无法确定变量是否被修改
    } else {
        const auto alloc = Alloc::create(gen_variable_name(), ir_type, cur_block);
        address = alloc;
        if (init_value->is_constant_init()) {
            std::static_pointer_cast<Init::Constant>(init_value)->gen_store_inst(address, cur_block);
        } else if (init_value->is_array_init()) {
            std::static_pointer_cast<Init::Array>(init_value)->gen_store_inst(address, cur_block, dimensions);
        } else {
            std::static_pointer_cast<Init::Exp>(init_value)->gen_store_inst(address, cur_block);
        }
    }
    table->insert_symbol(varDef->ident(), ir_type, init_value, address, false, is_global);
}

void Builder::visit_funcDef(const std::shared_ptr<AST::FuncDef> &funcDef) {
    const auto &ir_type = Type::get_type(funcDef->funcType());
    const auto &ident = funcDef->ident();
    // 检查标识符是否重定义，是否与库函数重名
    if (table->lookup_in_current_scope(ident)) { log_error("Redefinition of %s", ident.c_str()); }
    if (const auto it = Function::sysy_runtime_functions.find(ident); it != Function::sysy_runtime_functions.end()) {
        log_error("Redefinition of %s", ident.c_str());
    }
    if (const auto it = Function::llvm_runtime_functions.find(ident); it != Function::llvm_runtime_functions.end()) {
        log_error("Redefinition of %s", ident.c_str());
    }
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
        table->insert_symbol(ident, ir_type, nullptr, addr, false, true);
    }
    visit_block(funcDef->block());
    table->pop_scope();
    if (ident == "main") { module->set_main_function(func); }
    // 如果当前基本块没有终止指令，则插入默认返回语句
    auto cur_block_not_terminated = [&] {
        if (const auto &insts = cur_block->get_instructions(); insts.empty()) return true;
        else return std::dynamic_pointer_cast<Terminator>(insts.back()) == nullptr;
    };
    if (cur_block_not_terminated()) {
        if (const auto return_type = cur_function->get_return_type(); return_type->is_void()) {
            Ret::create(cur_block);
        } else if (return_type->is_int32()) {
            Ret::create(ConstInt::create(0), cur_block);
        } else if (return_type->is_float()) {
            Ret::create(ConstFloat::create(0.0f), cur_block);
        }
    }
    // 清除流图中无法到达的语句
    std::for_each(cur_function->get_blocks().begin(), cur_function->get_blocks().end(),
                  [&](const std::shared_ptr<Block> &block) {
                      auto &instructions = block->get_instructions();
                      auto find_terminator_index = [&] {
                          size_t i = 0;
                          for (; i < instructions.size() && std::dynamic_pointer_cast<Terminator>(instructions[i]) ==
                                 nullptr; ++i) {}
                          return i;
                      };
                      if (const auto l = find_terminator_index(); l < instructions.size() - 1) {
                          instructions.erase(instructions.begin() + static_cast<long>(l + 1), instructions.end());
                      }
                  });
    // 清除流图中无法到达的基本块
    std::unordered_set<std::shared_ptr<Block>> visited;
    auto dfs = [&](auto &&self, const std::shared_ptr<Block> &block) -> void {
        if (visited.count(block)) {
            return;
        }
        visited.insert(block);
        const auto &instructions = block->get_instructions();
        if (instructions.empty()) {
            log_error("Empty Block");
        }
        const auto last_instruction = instructions.back();
        if (const auto op = last_instruction->get_op(); op == Operator::JUMP) {
            self(self, last_instruction->as<Jump>()->get_target_block());
        } else if (op == Operator::BRANCH) {
            const auto branch = last_instruction->as<Branch>();
            self(self, branch->get_true_block());
            self(self, branch->get_false_block());
        } else if (op != Operator::RET) {
            log_error("Last instruction is not a terminator: %s", last_instruction->to_string().c_str());
        }
    };
    dfs(dfs, entry_block);
    auto &blocks = cur_function->get_blocks();
    blocks.erase(std::remove_if(blocks.begin(), blocks.end(), [&visited](const std::shared_ptr<Block> &block) {
        if (visited.find(block) != visited.end()) return false;
        std::for_each(block->get_instructions().begin(), block->get_instructions().end(),
                      [&](const std::shared_ptr<Instruction> &instruction) {
                          instruction->clear_operands();
                      });
        block->clear_operands();
        block->set_deleted();
        return true;
    }), blocks.end());
    cur_block = nullptr;
    cur_function = nullptr;
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
        int dim = res.get<int>();
        if (dim <= 0) { log_error("Non-Positive dimension: %d", dim); }
        dimensions.push_back(dim);
    }
    for (auto it = dimensions.rbegin(); it != dimensions.rend(); ++it) {
        const auto dim = *it;
        ir_type = Type::Array::create(dim, ir_type);
    }
    return {ident, Type::Pointer::create(ir_type)};
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
        const auto op = addExp->operators()[i - 1];
        if (lhs->get_type()->is_float() || rhs->get_type()->is_float()) {
            const auto &casted_lhs = type_cast(lhs, Type::Float::f32, cur_block),
                       &casted_rhs = type_cast(rhs, Type::Float::f32, cur_block);
            if (op == Token::Type::ADD) {
                lhs = FAdd::create(gen_variable_name(), casted_lhs, casted_rhs, cur_block);
            } else if (op == Token::Type::SUB) {
                lhs = FSub::create(gen_variable_name(), casted_lhs, casted_rhs, cur_block);
            }
        } else {
            const auto &casted_lhs = type_cast(lhs, Type::Integer::i32, cur_block),
                       &casted_rhs = type_cast(rhs, Type::Integer::i32, cur_block);
            if (op == Token::Type::ADD) {
                lhs = Add::create(gen_variable_name(), casted_lhs, casted_rhs, cur_block);
            } else if (op == Token::Type::SUB) {
                lhs = Sub::create(gen_variable_name(), casted_lhs, casted_rhs, cur_block);
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
        const auto op = mulExp->operators()[i - 1];
        if (lhs->get_type()->is_float() || rhs->get_type()->is_float()) {
            const auto &casted_lhs = type_cast(lhs, Type::Float::f32, cur_block),
                       &casted_rhs = type_cast(rhs, Type::Float::f32, cur_block);
            if (op == Token::Type::MUL) {
                lhs = FMul::create(gen_variable_name(), casted_lhs, casted_rhs, cur_block);
            } else if (op == Token::Type::DIV) {
                lhs = FDiv::create(gen_variable_name(), casted_lhs, casted_rhs, cur_block);
            } else if (op == Token::Type::MOD) {
                lhs = FMod::create(gen_variable_name(), casted_lhs, casted_rhs, cur_block);
            }
        } else {
            const auto &casted_lhs = type_cast(lhs, Type::Integer::i32, cur_block),
                       &casted_rhs = type_cast(rhs, Type::Integer::i32, cur_block);
            if (op == Token::Type::MUL) {
                lhs = Mul::create(gen_variable_name(), casted_lhs, casted_rhs, cur_block);
            } else if (op == Token::Type::DIV) {
                lhs = Div::create(gen_variable_name(), casted_lhs, casted_rhs, cur_block);
            } else if (op == Token::Type::MOD) {
                lhs = Mod::create(gen_variable_name(), casted_lhs, casted_rhs, cur_block);
            }
        }
    }
    return lhs;
}

std::shared_ptr<Value> Builder::visit_functionCall(const AST::UnaryExp::call &call) const {
    const auto &[ident, params] = call;
    auto func = module->get_function(ident.content);
    if (!func) {
        if (const auto it = Function::sysy_runtime_functions.find(ident.content);
            it != Function::sysy_runtime_functions.end()) {
            func = it->second;
            module->add_used_runtime_functions(func);
        } else {
            log_error("Unknown function: %s", ident.content.c_str());
        }
    }
    // 实参列表
    std::vector<std::shared_ptr<Value>> r_params;
    if (ident.content == "starttime" || ident.content == "stoptime") {
        r_params.emplace_back(ConstInt::create(ident.line));
        return Call::create(func, r_params, cur_block);
    }
    if (ident.content == "putf") {
        if (!params[0]->is_const_string()) { log_fatal("First parameter of putf must be a const string"); }
        const auto &const_string = params[0]->get_const_string();
        for (size_t i = 1; i < params.size(); ++i) {
            if (params[i]->is_const_string()) { log_fatal("Parameter should not be a const string"); }
            r_params.emplace_back(visit_exp(params[i]));
        }
        module->add_const_string(const_string);
        return Call::create(func, r_params, cur_block, static_cast<int>(module->get_const_string_size()));
    }
    const auto &arguments = func->get_arguments();
    if (params.size() != arguments.size()) {
        log_error("Function %s has %zu arguments, but %zu parameters are provided at line %d", ident.content.c_str(),
                  arguments.size(), params.size(), ident.line);
    }
    for (size_t i = 0; i < params.size(); ++i) {
        if (params[i]->is_const_string()) { log_fatal("Parameter should not be a const string"); }
        if (const auto f_type = arguments[i]->get_type(); f_type->is_int32() || f_type->is_float()) {
            r_params.emplace_back(type_cast(visit_exp(params[i]), f_type, cur_block));
        } else {
            std::shared_ptr<Value> p = visit_exp(params[i]);
            if (*p->get_type() != *f_type) {
                p = BitCast::create(gen_variable_name(), p, f_type, cur_block);
            }
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
                const auto &casted_sub_exp_value = type_cast(sub_exp_value, Type::Float::f32, cur_block);
                return FSub::create(gen_variable_name(), ConstFloat::create(0.0f),
                                    casted_sub_exp_value, cur_block);
            }
            const auto &casted_sub_exp_value = type_cast(sub_exp_value, Type::Integer::i32, cur_block);
            return Sub::create(gen_variable_name(), ConstInt::create(0),
                               casted_sub_exp_value, cur_block);
        }
        if (type == Token::Type::NOT) {
            if (sub_exp_value->get_type()->is_float()) {
                const auto &casted_sub_exp_value = type_cast(sub_exp_value, Type::Float::f32, cur_block);
                const auto &cmp = Fcmp::create(gen_variable_name(), Fcmp::Op::EQ,
                                               casted_sub_exp_value,
                                               ConstFloat::create(0.0f), cur_block);
                return type_cast(cmp, Type::Integer::i32, cur_block);
            }
            const auto &casted_sub_exp_value = type_cast(sub_exp_value, Type::Integer::i32, cur_block);
            const auto &cmp = Icmp::create(gen_variable_name(), Icmp::Op::EQ,
                                           casted_sub_exp_value,
                                           ConstInt::create(0), cur_block);
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
        return ConstFloat::create(num->get_value());
    }
    if (const auto &num = std::dynamic_pointer_cast<AST::IntNumber>(number)) {
        return ConstInt::create(num->get_value());
    }
    log_fatal("Invalid number");
}

std::shared_ptr<Value> Builder::visit_lVal(const std::shared_ptr<AST::LVal> &lVal, const bool get_address) const {
    const auto &ident = lVal->ident();
    const auto symbol = table->lookup_in_all_scopes(ident);
    if (!symbol) { log_error("Undefined variable: %s", ident.c_str()); }
    const auto address = symbol->get_address();
    if (!address->get_type()->is_pointer()) {
        log_fatal("Invalid address type %s of %s", address->get_type()->to_string().c_str(), ident.c_str());
    }

    const auto is_symbol_unmodified = [&]() -> bool {
        // 常量是“未被修改”的
        if (symbol->is_constant_symbol()) return true;
        // 已标记“被修改”
        if (symbol->is_modified_symbol()) return false;
        // 在if或while语句块中，由于路径不确定，故标记为“被修改”
        if (!loop_stats.empty() || !cond_stats.empty()) {
            symbol->set_modified();
            return false;
        }
        // 如果是全局变量，默认是“被修改”的
        return std::dynamic_pointer_cast<GlobalVariable>(address) == nullptr;
    };

    const auto ir_type = std::static_pointer_cast<Type::Pointer>(address->get_type())->get_contain_type();
    if (!get_address && (ir_type->is_int32() || ir_type->is_float()) && lVal->exps().empty()) {
        if (is_symbol_unmodified()) {
            const auto initial = symbol->get_init_value();
            if (const auto constant_initial = std::dynamic_pointer_cast<Init::Constant>(initial)) {
                return constant_initial->get_const_value();
            }
            if (const auto exp_initial = std::dynamic_pointer_cast<Init::Exp>(initial);
                exp_initial != nullptr && !symbol->is_modified_symbol()) {
                return exp_initial->get_exp_value();
            }
        }
        return Load::create(gen_variable_name(), address, cur_block);
    }

    std::vector<std::shared_ptr<Value>> indexes;
    auto content_type = ir_type;
    bool all_index_constant = true;
    for (const auto &exp: lVal->exps()) {
        const auto &idx_value = type_cast(visit_exp(exp), Type::Integer::i32, cur_block);
        if (!idx_value->is_constant()) {
            all_index_constant = false;
        }
        indexes.emplace_back(idx_value);
        if (content_type->is_pointer()) {
            content_type = std::static_pointer_cast<Type::Pointer>(content_type)->get_contain_type();
        } else if (content_type->is_array()) {
            content_type = std::static_pointer_cast<Type::Array>(content_type)->get_element_type();
        } else {
            log_error("Invalid content type %s", content_type->to_string().c_str());
        }
    }

    if (all_index_constant && is_symbol_unmodified() && !get_address && !indexes.empty() && !content_type->is_array()) {
        auto initial = symbol->get_init_value();
        if (const auto array_init = std::dynamic_pointer_cast<Init::Array>(initial)) {
            std::vector<int> int_indexes;
            int_indexes.reserve(indexes.size());
            for (const auto &idx: indexes) {
                int_indexes.push_back(idx->as<ConstInt>()->get<int>());
            }
            initial = array_init->get_init_value(int_indexes);
            if (const auto constant_initial = std::dynamic_pointer_cast<Init::Constant>(initial)) {
                return constant_initial->get_const_value();
            }
            if (const auto exp_initial = std::dynamic_pointer_cast<Init::Exp>(initial)) {
                return exp_initial->get_exp_value();
            }
        }
    }

    std::shared_ptr<Value> pointer = address;
    content_type = std::static_pointer_cast<Type::Pointer>(address->get_type())->get_contain_type();
    for (const auto &index: indexes) {
        if (content_type->is_pointer()) {
            pointer = Load::create(gen_variable_name(), pointer, cur_block);
            content_type = std::static_pointer_cast<Type::Pointer>(content_type)->get_contain_type();
            pointer = GetElementPtr::create(gen_variable_name(), pointer, {index}, cur_block);
        } else if (content_type->is_array()) {
            content_type = std::static_pointer_cast<Type::Array>(content_type)->get_element_type();
            pointer = GetElementPtr::create(gen_variable_name(), pointer,
                                            {ConstInt::create(0), index}, cur_block);
        }
    }
    if (get_address) {
        // 只有在解析assignStmt时，get_address才为true
        // 认为取地址导致symbol的初始值被修改
        symbol->set_modified();
        return pointer;
    }
    if (content_type->is_int32() || content_type->is_float() || content_type->is_pointer()) {
        return Load::create(gen_variable_name(), pointer, cur_block);
    }
    if (content_type->is_array()) {
        // 解析向函数中调用数组指针的情况
        // 此时也认为symbol的初始值被修改
        symbol->set_modified();
        const auto constant_zero = ConstInt::create(0);
        return GetElementPtr::create(gen_variable_name(), pointer, {constant_zero, constant_zero}, cur_block);
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
            const auto &casted_lhs = type_cast(lhs, Type::Float::f32, cur_block),
                       &casted_rhs = type_cast(rhs, Type::Float::f32, cur_block);
            if (const auto op = eqExp->operators()[i - 1]; op == Token::Type::EQ) {
                lhs = Fcmp::create(gen_variable_name(), Fcmp::Op::EQ, casted_lhs, casted_rhs, cur_block);
            } else if (op == Token::Type::NE) {
                lhs = Fcmp::create(gen_variable_name(), Fcmp::Op::NE, casted_lhs, casted_rhs, cur_block);
            }
        } else {
            const auto &casted_lhs = type_cast(lhs, Type::Integer::i32, cur_block),
                       &casted_rhs = type_cast(rhs, Type::Integer::i32, cur_block);
            if (const auto op = eqExp->operators()[i - 1]; op == Token::Type::EQ) {
                lhs = Icmp::create(gen_variable_name(), Icmp::Op::EQ, casted_lhs, casted_rhs, cur_block);
            } else if (op == Token::Type::NE) {
                lhs = Icmp::create(gen_variable_name(), Icmp::Op::NE, casted_lhs, casted_rhs, cur_block);
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
            const auto &casted_lhs = type_cast(lhs, Type::Float::f32, cur_block),
                       &casted_rhs = type_cast(rhs, Type::Float::f32, cur_block);
            if (const auto op = relExp->operators()[i - 1]; op == Token::Type::LT) {
                lhs = Fcmp::create(gen_variable_name(), Fcmp::Op::LT, casted_lhs, casted_rhs, cur_block);
            } else if (op == Token::Type::GT) {
                lhs = Fcmp::create(gen_variable_name(), Fcmp::Op::GT, casted_lhs, casted_rhs, cur_block);
            } else if (op == Token::Type::LE) {
                lhs = Fcmp::create(gen_variable_name(), Fcmp::Op::LE, casted_lhs, casted_rhs, cur_block);
            } else if (op == Token::Type::GE) {
                lhs = Fcmp::create(gen_variable_name(), Fcmp::Op::GE, casted_lhs, casted_rhs, cur_block);
            }
        } else {
            const auto &casted_lhs = type_cast(lhs, Type::Integer::i32, cur_block),
                       &casted_rhs = type_cast(rhs, Type::Integer::i32, cur_block);
            if (const auto op = relExp->operators()[i - 1]; op == Token::Type::LT) {
                lhs = Icmp::create(gen_variable_name(), Icmp::Op::LT, casted_lhs, casted_rhs, cur_block);
            } else if (op == Token::Type::GT) {
                lhs = Icmp::create(gen_variable_name(), Icmp::Op::GT, casted_lhs, casted_rhs, cur_block);
            } else if (op == Token::Type::LE) {
                lhs = Icmp::create(gen_variable_name(), Icmp::Op::LE, casted_lhs, casted_rhs, cur_block);
            } else if (op == Token::Type::GE) {
                lhs = Icmp::create(gen_variable_name(), Icmp::Op::GE, casted_lhs, casted_rhs, cur_block);
            }
        }
    }
    if (!lhs->get_type()->is_integer() && !lhs->get_type()->is_float()) {
        log_error("Invalid relExp ", lhs->to_string().c_str());
    }
    return lhs;
}

void Builder::visit_stmt(const std::shared_ptr<AST::Stmt> &stmt) {
    if (const auto blockStmt = std::dynamic_pointer_cast<AST::BlockStmt>(stmt)) {
        visit_blockStmt(blockStmt);
    } else if (const auto assignStmt = std::dynamic_pointer_cast<AST::AssignStmt>(stmt)) {
        visit_assignStmt(assignStmt);
    } else if (const auto expStmt = std::dynamic_pointer_cast<AST::ExpStmt>(stmt)) {
        visit_expStmt(expStmt);
    } else if (const auto returnStmt = std::dynamic_pointer_cast<AST::ReturnStmt>(stmt)) {
        visit_returnStmt(returnStmt);
    } else if (const auto ifStmt = std::dynamic_pointer_cast<AST::IfStmt>(stmt)) {
        visit_ifStmt(ifStmt);
    } else if (const auto whileStmt = std::dynamic_pointer_cast<AST::WhileStmt>(stmt)) {
        visit_whileStmt(whileStmt);
    } else if (auto breakStmt = std::dynamic_pointer_cast<AST::BreakStmt>(stmt)) {
        visit_breakStmt();
    } else if (auto continueStmt = std::dynamic_pointer_cast<AST::ContinueStmt>(stmt)) {
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
    const auto exp_value = visit_exp(assignStmt->exp());
    const auto address = visit_lVal(assignStmt->lVal(), true);
    if (!address->get_type()->is_pointer()) {
        log_error("Invalid address type %s", address->get_type()->to_string().c_str());
    }
    const auto ir_type = std::static_pointer_cast<Type::Pointer>(address->get_type())->get_contain_type();
    if (!ir_type->is_int32() && !ir_type->is_float()) {
        log_error("Invalid element type %s", ir_type->to_string().c_str());
    }
    const auto casted_exp_value = type_cast(exp_value, ir_type, cur_block);
    Store::create(address, casted_exp_value, cur_block);
}

void Builder::visit_expStmt(const std::shared_ptr<AST::ExpStmt> &expStmt) const {
    if (const auto exp = expStmt->exp()) {
        // ReSharper disable once CppExpressionWithoutSideEffects
        visit_exp(exp);
    }
}

void Builder::visit_returnStmt(const std::shared_ptr<AST::ReturnStmt> &returnStmt) const {
    if (const auto exp = returnStmt->exp()) {
        if (cur_function->get_return_type()->is_void()) { log_error("Cannot return value in void function"); }
        const auto exp_value = visit_exp(exp);
        const auto casted_exp_value = type_cast(exp_value, cur_function->get_return_type(), cur_block);
        Ret::create(casted_exp_value, cur_block);
    } else {
        if (!cur_function->get_return_type()->is_void()) { log_error("Cannot return void value in non-void function"); }
        Ret::create(cur_block);
    }
}

void Builder::visit_ifStmt(const std::shared_ptr<AST::IfStmt> &ifStmt) {
    const auto then_block = Block::create(gen_block_name(), cur_function);
    cond_stats.emplace_back(ifStmt->cond());
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
    cond_stats.pop_back();
}

void Builder::visit_whileStmt(const std::shared_ptr<AST::WhileStmt> &whileStmt) {
    auto cond_block = Block::create(gen_block_name(), cur_function),
         body_block = Block::create(gen_block_name(), cur_function),
         follow_block = Block::create(gen_block_name(), cur_function);
    loop_stats.emplace_back(cond_block, body_block, follow_block);
    cond_stats.emplace_back(whileStmt->cond());
    Jump::create(cond_block, cur_block);
    cur_block = cond_block;
    visit_cond(whileStmt->cond(), body_block, follow_block);
    cur_block = body_block;
    visit_stmt(whileStmt->body());
    visit_cond(whileStmt->cond(), body_block, follow_block);
    cur_block = follow_block;
    loop_stats.pop_back();
    cond_stats.pop_back();
}

void Builder::visit_breakStmt() {
    auto [cond_block, body_block, follow_block] = loop_stats.back();
    Jump::create(follow_block, cur_block);
}

void Builder::visit_continueStmt() {
    auto [cond_block, body_block, follow_block] = loop_stats.back();
    Jump::create(cond_block, cur_block);
}
}
