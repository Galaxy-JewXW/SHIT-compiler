#include "Mir/Init.h"
#include "Mir/Instruction.h"
#include "Mir/Structure.h"
#include "Utils/AST.h"
#include "Utils/Token.h"

template<typename T>
void join_and_append(std::ostringstream &oss, const std::vector<std::shared_ptr<T>> &items,
                     const std::string &separator) {
    for (size_t i = 0; i < items.size(); ++i) {
        oss << items[i]->to_string();
        if (i != items.size() - 1) {
            oss << separator;
        }
    }
}

std::string Token::type_to_string(const Type type) {
    switch (type) {
        // 关键词
        case Type::CONST: return "CONST";
        case Type::INT: return "INT";
        case Type::FLOAT: return "FLOAT";
        case Type::VOID: return "VOID";
        case Type::IF: return "IF";
        case Type::ELSE: return "ELSE";
        case Type::WHILE: return "WHILE";
        case Type::BREAK: return "BREAK";
        case Type::CONTINUE: return "CONTINUE";
        case Type::RETURN: return "RETURN";

        // 标识符
        case Type::IDENTIFIER: return "IDENTIFIER";

        // 字面量
        case Type::INT_CONST: return "INT_CONST";
        case Type::FLOAT_CONST: return "FLOAT_CONST";
        case Type::STRING_CONST: return "STRING_CONST";

        // 运算符
        case Type::ADD: return "ADD";
        case Type::SUB: return "SUB";
        case Type::NOT: return "NOT";
        case Type::MUL: return "MUL";
        case Type::DIV: return "DIV";
        case Type::MOD: return "MOD";
        case Type::LT: return "LT";
        case Type::GT: return "GT";
        case Type::LE: return "LE";
        case Type::GE: return "GE";
        case Type::EQ: return "EQ";
        case Type::NE: return "NE";
        case Type::AND: return "AND";
        case Type::OR: return "OR";

        // 分隔符
        case Type::SEMICOLON: return "SEMICOLON";
        case Type::COMMA: return "COMMA";
        case Type::ASSIGN: return "ASSIGN";
        case Type::LPAREN: return "LPAREN";
        case Type::RPAREN: return "RPAREN";
        case Type::LBRACE: return "LBRACE";
        case Type::RBRACE: return "RBRACE";
        case Type::LBRACKET: return "LBRACKET";
        case Type::RBRACKET: return "RBRACKET";

        // 结束符
        case Type::END_OF_FILE: return "EOF";

        // 未知
        default: return "UNKNOWN";
    }
}

[[nodiscard]] std::string Token::Token::to_string() const {
    std::ostringstream oss;
    oss << "{" << line << " " << type_to_string(type) << " \"" << content << "\"}";
    return oss.str();
}

namespace AST {
[[nodiscard]] std::string CompUnit::to_string() const {
    std::ostringstream oss;
    for (auto &unit: compunits_) {
        std::visit([&oss](auto &item) {
            oss << item->to_string() << "\n";
        }, unit);
    }
    oss << "<CompUnit>\n";
    return oss.str();
}

[[nodiscard]] std::string ConstDecl::to_string() const {
    std::ostringstream oss;
    oss << "<bType " << type_to_string(bType_) << ">\n";
    for (const auto &constDef: constDefs_) {
        oss << constDef->to_string() << "\n";
    }
    oss << "<ConstDecl>";
    return oss.str();
}

[[nodiscard]] std::string ConstDef::to_string() const {
    std::ostringstream oss;
    oss << "<Ident " << ident_ << ">\n";
    if (!constExps_.empty()) {
        for (const auto &constExp: constExps_) {
            oss << "[\n" << constExp->to_string() << "\n]\n";
        }
    }
    oss << "=\n" << constInitVal_->to_string() << "\n";
    oss << "<ConstDef>";
    return oss.str();
}

[[nodiscard]] std::string ConstInitVal::to_string() const {
    std::ostringstream oss;
    if (is_constExp()) {
        const auto &constExp = std::get<std::shared_ptr<ConstExp>>(value_);
        oss << constExp->to_string() << "\n";
    } else if (is_constInitVals()) {
        const auto &constInitVals
                = std::get<std::vector<std::shared_ptr<ConstInitVal>>>(value_);
        oss << "{\n";
        for (size_t i = 0u; i < constInitVals.size(); ++i) {
            oss << constInitVals[i]->to_string();
            if (i != constInitVals.size() - 1) {
                oss << "\n,";
            }
            oss << "\n";
        }
        oss << "}\n";
    } else {
        throw std::runtime_error("Invalid ConstInitVal");
    }
    oss << "<ConstInitVal>";
    return oss.str();
}

[[nodiscard]] std::string VarDecl::to_string() const {
    std::ostringstream oss;
    oss << "<bType " << type_to_string(bType_) << ">\n";
    for (const auto &varDef: varDefs_) {
        oss << varDef->to_string() << "\n";
    }
    oss << "<VarDecl>";
    return oss.str();
}

[[nodiscard]] std::string VarDef::to_string() const {
    std::ostringstream oss;
    oss << "<Ident " << ident_ << ">\n";
    if (!constExps_.empty()) {
        for (const auto &constExp: constExps_) {
            oss << "[\n" << constExp->to_string() << "\n]\n";
        }
    }
    if (initVal_ != nullptr) {
        oss << "=\n" << initVal_->to_string() << "\n";
    }
    oss << "<VarDef>";
    return oss.str();
}

[[nodiscard]] std::string InitVal::to_string() const {
    std::ostringstream oss;
    if (is_exp()) {
        const auto &exp = std::get<std::shared_ptr<Exp>>(value_);
        oss << exp->to_string() << "\n";
    } else if (is_initVals()) {
        const auto &initVals
                = std::get<std::vector<std::shared_ptr<InitVal>>>(value_);
        oss << "{\n";
        for (size_t i = 0u; i < initVals.size(); ++i) {
            oss << initVals[i]->to_string();
            if (i != initVals.size() - 1) {
                oss << "\n,";
            }
            oss << "\n";
        }
        oss << "}\n";
    } else {
        throw std::runtime_error("Invalid InitVal");
    }
    oss << "<InitVal>";
    return oss.str();
}

[[nodiscard]] std::string FuncDef::to_string() const {
    std::ostringstream oss;
    oss << "<FuncType " << type_to_string(funcType_) << ">\n";
    oss << "<Ident " << ident_ << ">\n";
    if (!funcParams_.empty()) {
        oss << "(\n";
        for (size_t i = 0u; i < funcParams_.size(); ++i) {
            oss << funcParams_[i]->to_string() << "\n";
            if (i != funcParams_.size() - 1) {
                oss << ",\n";
            }
        }
        oss << ")\n";
    }
    oss << block_->to_string() << "\n";
    oss << "<FuncDef>";
    return oss.str();
}

[[nodiscard]] std::string FuncFParam::to_string() const {
    std::ostringstream oss;
    oss << "<bType " << type_to_string(bType_) << ">\n";
    oss << "<Ident " << ident_ << ">\n";
    if (!exps_.empty()) {
        oss << "[\n]\n";
        for (const auto &exp: exps_) {
            if (exp != nullptr) {
                oss << "[\n" << exp->to_string() << "\n]\n";
            }
        }
    }
    oss << "<FuncFParam>";
    return oss.str();
}

[[nodiscard]] std::string Block::to_string() const {
    std::ostringstream oss;
    for (auto &unit: items_) {
        std::visit([&oss](const auto &item) {
            oss << item->to_string() << "\n";
        }, unit);
    }
    oss << "<Block>";
    return oss.str();
}

[[nodiscard]] std::string AssignStmt::to_string() const {
    std::ostringstream oss;
    oss << lVal_->to_string() << "\n=\n" << exp_->to_string() << "\n<AssignStmt>";
    return oss.str();
}

[[nodiscard]] std::string ExpStmt::to_string() const {
    std::ostringstream oss;
    if (exp_ != nullptr) {
        oss << exp_->to_string() << "\n<ExpStmt>";
    }
    return oss.str();
}

[[nodiscard]] std::string BlockStmt::to_string() const {
    std::ostringstream oss;
    oss << block_->to_string() << "\n<BlockStmt>";
    return oss.str();
}

[[nodiscard]] std::string IfStmt::to_string() const {
    std::ostringstream oss;
    oss << "if\n";
    oss << cond_->to_string() << "\n";
    oss << "then\n";
    oss << then_->to_string() << "\n";
    if (else_ != nullptr) {
        oss << "else\n";
        oss << else_->to_string() << "\n";
    }
    oss << "<IfStmt>";
    return oss.str();
}

[[nodiscard]] std::string WhileStmt::to_string() const {
    std::ostringstream oss;
    oss << "while\n";
    oss << cond_->to_string() << "\n";
    oss << body_->to_string() << "\n";
    oss << "<WhileStmt>";
    return oss.str();
}

[[nodiscard]] std::string BreakStmt::to_string() const {
    return "<BreakStmt>";
}

[[nodiscard]] std::string ContinueStmt::to_string() const {
    return "<ContinueStmt>";
}

[[nodiscard]] std::string ReturnStmt::to_string() const {
    std::ostringstream oss;
    oss << "return\n";
    if (exp_ != nullptr) {
        oss << exp_->to_string() << "\n";
    }
    oss << "<ReturnStmt>";
    return oss.str();
}

[[nodiscard]] std::string Exp::to_string() const {
    std::ostringstream oss;
    if (std::holds_alternative<std::shared_ptr<AddExp>>(addExp_)) {
        oss << std::get<std::shared_ptr<AddExp>>(addExp_)->to_string() << "\n<Exp>";
    } else {
        oss << std::get<std::string>(addExp_) << "\n<ConstString>";
    }
    return oss.str();
}

[[nodiscard]] std::string Cond::to_string() const {
    std::ostringstream oss;
    oss << lOrExp_->to_string() << "\n<Cond>";
    return oss.str();
}

[[nodiscard]] std::string LVal::to_string() const {
    std::ostringstream oss;
    oss << "<Ident " << ident_ << ">\n";
    if (!exps_.empty()) {
        for (const auto &exp: exps_) {
            oss << "[\n" << exp->to_string() << "\n]\n";
        }
    }
    oss << "<LVal>";
    return oss.str();
}

[[nodiscard]] std::string PrimaryExp::to_string() const {
    std::ostringstream oss;
    if (is_exp()) {
        const auto &exp = std::get<std::shared_ptr<Exp>>(value_);
        oss << "(\n" + exp->to_string() << "\n)\n";
    } else if (is_lVal()) {
        const auto &lVal = std::get<std::shared_ptr<LVal>>(value_);
        oss << lVal->to_string() << "\n";
    } else if (is_number()) {
        const auto &number = std::get<std::shared_ptr<Number>>(value_);
        oss << number->to_string() << "\n";
    } else {
        throw std::runtime_error("Invalid PrimaryExp");
    }
    oss << "<PrimaryExp>";
    return oss.str();
}

[[nodiscard]] std::string IntNumber::to_string() const {
    return std::to_string(value_);
}

[[nodiscard]] std::string FloatNumber::to_string() const {
    return std::to_string(value_);
}

[[nodiscard]] std::string UnaryExp::to_string() const {
    std::ostringstream oss;
    if (is_primaryExp()) {
        const auto &primaryExp = std::get<std::shared_ptr<PrimaryExp>>(value_);
        oss << primaryExp->to_string() << "\n";
    } else if (is_call()) {
        const auto &[ident, params] = std::get<call>(value_);
        oss << "<Ident " << ident.content << ">\n";
        if (!params.empty()) {
            oss << "(\n";
            for (size_t i = 0u; i < params.size(); ++i) {
                oss << params[i]->to_string() << "\n";
                if (i != params.size() - 1) {
                    oss << ",\n";
                }
            }
            oss << ")\n";
        }
    } else if (is_opExp()) {
        const auto &[type, unaryExp] = std::get<opExp>(value_);
        oss << "<UnaryOp " << type_to_string(type) << ">\n";
        oss << unaryExp->to_string() << "\n";
    } else {
        throw std::runtime_error("Invalid UnaryExp");
    }
    oss << "<UnaryExp>";
    return oss.str();
}

[[nodiscard]] std::string MulExp::to_string() const {
    std::ostringstream oss;
    for (size_t i = 0u; i < unaryExps_.size(); ++i) {
        if (i > 0) {
            oss << "<" << type_to_string(operators_[i - 1]) << ">\n";
        }
        oss << unaryExps_[i]->to_string() << "\n";
    }
    oss << "<MulExp>";
    return oss.str();
}

[[nodiscard]] std::string AddExp::to_string() const {
    std::ostringstream oss;
    for (size_t i = 0u; i < mulExps_.size(); ++i) {
        if (i > 0) {
            oss << "<" << type_to_string(operators_[i - 1]) << ">\n";
        }
        oss << mulExps_[i]->to_string() << "\n";
    }
    oss << "<AddExp>";
    return oss.str();
}

[[nodiscard]] std::string RelExp::to_string() const {
    std::ostringstream oss;
    for (size_t i = 0u; i < addExps_.size(); ++i) {
        if (i > 0) {
            oss << "<" << type_to_string(operators_[i - 1]) << ">\n";
        }
        oss << addExps_[i]->to_string() << "\n";
    }
    oss << "<RelExp>";
    return oss.str();
}

[[nodiscard]] std::string EqExp::to_string() const {
    std::ostringstream oss;
    for (size_t i = 0u; i < relExps_.size(); ++i) {
        if (i > 0) {
            oss << "<" << type_to_string(operators_[i - 1]) << ">\n";
        }
        oss << relExps_[i]->to_string() << "\n";
    }
    oss << "<EqExp>";
    return oss.str();
}

[[nodiscard]] std::string LAndExp::to_string() const {
    std::ostringstream oss;
    for (size_t i = 0u; i < eqExps_.size(); ++i) {
        if (i > 0) {
            oss << "<&&>\n";
        }
        oss << eqExps_[i]->to_string() << "\n";
    }
    oss << "<LAndExp>";
    return oss.str();
}

[[nodiscard]] std::string LOrExp::to_string() const {
    std::ostringstream oss;
    for (size_t i = 0u; i < lAndExps_.size(); ++i) {
        if (i > 0) {
            oss << "<||>\n";
        }
        oss << lAndExps_[i]->to_string() << "\n";
    }
    oss << "<LOrExp>";
    return oss.str();
}

[[nodiscard]] std::string ConstExp::to_string() const {
    std::ostringstream oss;
    oss << addExp_->to_string() << "\n<ConstExp>";
    return oss.str();
}
}

std::string str_to_llvm_ir(const std::string &str) {
    auto s = str;
    const auto l = s.size();
    size_t pos = 0;
    while ((pos = s.find("\\n", pos)) != std::string::npos) {
        s.replace(pos, 2, "\\0A");
    }
    return std::to_string(l) + " x i8] c\"" + s + "\\00\", align 1";
}

namespace Mir {
[[nodiscard]] std::string Module::to_string() const {
    std::ostringstream oss;
    for (size_t i = 0; i < const_strings.size(); ++i) {
        oss << "@.str_" << i + 1 << " = private unnamed_addr constant [" << str_to_llvm_ir(const_strings[i]) << "\n";
    }
    if (!const_strings.empty()) { oss << "\n"; }
    join_and_append(oss, used_runtime_functions, "\n");
    if (!used_runtime_functions.empty()) { oss << "\n"; }
    // 拼接全局变量
    join_and_append(oss, global_variables, "\n");
    if (!global_variables.empty()) { oss << "\n"; }
    // 拼接函数
    join_and_append(oss, functions, "\n");
    if (!functions.empty()) { oss << "\n"; }
    oss << "\ndeclare void @llvm.memset.p0i8.i32(i8* nocapture writeonly, i8, i32, i1 immarg)\n";
    return oss.str();
}


[[nodiscard]] std::string GlobalVariable::to_string() const {
    std::ostringstream oss;
    oss << name_ << " = dso_local " << (is_constant ? "constant " : "global ")
            << init_value->to_string();
    return oss.str();
}

[[nodiscard]] std::string Function::to_string() const {
    if (is_runtime_function) {
        if (name_ == "putf")
            return "declare void @putf(i8*, ...)";
        std::ostringstream oss;
        oss << "declare " << type_->to_string() << " @" << name_ << "(";
        for (size_t i = 0; i < arguments.size(); ++i) {
            oss << arguments[i]->get_type()->to_string();
            if (i != arguments.size() - 1) {
                oss << ", ";
            }
        }
        oss << ")";
        return oss.str();
    }
    std::ostringstream param_info;
    for (size_t i = 0; i < arguments.size(); ++i) {
        param_info << arguments[i]->to_string();
        if (i != arguments.size() - 1) {
            param_info << ", ";
        }
    }
    std::ostringstream block_info;
    for (size_t i = 0; i < blocks.size(); ++i) {
        block_info << blocks[i]->to_string();
        if (i != blocks.size() - 1) {
            block_info << "\n";
        }
    }
    std::ostringstream function_info;
    function_info << "define dso_local " << type_->to_string() << " @" << name_
            << "(" << param_info.str() << ") {\n"
            << block_info.str()
            << "\n}";
    return function_info.str();
}

[[nodiscard]] std::string Block::to_string() const {
    std::ostringstream oss;
    oss << name_ << ":\n\t";
    for (size_t i = 0; i < instructions.size(); ++i) {
        oss << instructions[i]->to_string();
        if (i != instructions.size() - 1) {
            oss << "\n\t";
        }
    }
    return oss.str();
}

[[nodiscard]] std::string Alloc::to_string() const {
    std::ostringstream oss;
    oss << name_ << " = ";
    const auto &type = std::static_pointer_cast<Type::Pointer>(type_);
    oss << "alloca " << type->get_contain_type()->to_string();
    return oss.str();
}

[[nodiscard]] std::string Load::to_string() const {
    std::ostringstream oss;
    oss << name_ << " = ";
    oss << "load " << type_->to_string() << ", ";
    const auto &addr = get_addr();
    oss << addr->get_type()->to_string() << " " << addr->get_name();
    return oss.str();
}

[[nodiscard]] std::string Store::to_string() const {
    std::ostringstream oss;
    oss << "store ";
    const auto &addr = get_addr(), &value = get_value();
    oss << value->get_type()->to_string() << " " << value->get_name() << ", ";
    oss << addr->get_type()->to_string() << " " << addr->get_name();
    return oss.str();
}

[[nodiscard]] std::string GetElementPtr::to_string() const {
    const auto addr = get_addr();
    const auto ptr_type = std::static_pointer_cast<Type::Pointer>(addr->get_type());
    const auto target_type = ptr_type->get_contain_type();
    std::ostringstream oss;
    oss << name_ << " = getelementptr inbounds " << target_type->to_string()
            << ", " << ptr_type->to_string() << " " << addr->get_name();
    for (size_t i = 1; i < operands_.size(); ++i) {
        oss << ", " << operands_[i]->get_type()->to_string() << " " << operands_[i]->get_name();
    }
    return oss.str();
}

[[nodiscard]] std::string BitCast::to_string() const {
    std::ostringstream oss;
    oss << name_ << " = bitcast ";
    const auto &origin_value = get_value();
    oss << origin_value->get_type()->to_string() << " " << origin_value->get_name();
    oss << " to " << type_->to_string();
    return oss.str();
}


[[nodiscard]] std::string Fptosi::to_string() const {
    std::ostringstream oss;
    oss << name_ << " = fptosi ";
    const auto &origin_value = get_value();
    oss << origin_value->get_type()->to_string() << " " << origin_value->get_name();
    oss << " to " << type_->to_string();
    return oss.str();
}

[[nodiscard]] std::string Sitofp::to_string() const {
    std::ostringstream oss;
    oss << name_ << " = sitofp ";
    const auto &origin_value = get_value();
    oss << origin_value->get_type()->to_string() << " " << origin_value->get_name();
    oss << " to " << type_->to_string();
    return oss.str();
}

[[nodiscard]] std::string Fcmp::to_string() const {
    const auto op_str = [&] {
        const std::string id = "fcmp";
        switch (op) {
            case Op::EQ: return id + " oeq";
            case Op::NE: return id + " one";
            case Op::LT: return id + " olt";
            case Op::LE: return id + " ole";
            case Op::GT: return id + " ogt";
            case Op::GE: return id + " oge";
            default: log_error("Unknown op");
        }
    }();
    std::ostringstream oss;
    oss << name_ << " = " << op_str << " float ";
    oss << get_lhs()->get_name() << ", " << get_rhs()->get_name();
    return oss.str();
}

[[nodiscard]] std::string Icmp::to_string() const {
    const auto op_str = [&] {
        const std::string id = "icmp";
        switch (op) {
            case Op::EQ: return id + " eq";
            case Op::NE: return id + " ne";
            case Op::LT: return id + " slt";
            case Op::LE: return id + " sle";
            case Op::GT: return id + " sgt";
            case Op::GE: return id + " sge";
            default: log_error("Unknown op");
        }
    }();
    std::ostringstream oss;
    oss << name_ << " = " << op_str << " i32 ";
    oss << get_lhs()->get_name() << ", " << get_rhs()->get_name();
    return oss.str();
}

[[nodiscard]] std::string Zext::to_string() const {
    std::ostringstream oss;
    oss << name_ << " = zext ";
    const auto &origin_value = get_value();
    oss << origin_value->get_type()->to_string() << " " << origin_value->get_name();
    oss << " to " << type_->to_string();
    return oss.str();
}

[[nodiscard]] std::string Branch::to_string() const {
    std::ostringstream oss;
    oss << "br ";
    const auto &cond = get_cond();
    oss << cond->get_type()->to_string() << " " << cond->get_name() << ", ";
    oss << "label %" << get_true_block()->get_name() << ", label %" << get_false_block()->get_name();
    return oss.str();
}

[[nodiscard]] std::string Jump::to_string() const {
    std::ostringstream oss;
    oss << "br label %" << get_target_block()->get_name();
    return oss.str();
}

[[nodiscard]] std::string Ret::to_string() const {
    std::ostringstream oss;
    oss << "ret ";
    if (operands_.empty()) {
        oss << "void";
    } else {
        oss << get_value()->get_type()->to_string() << " " << get_value()->get_name();
    }
    return oss.str();
}

[[nodiscard]] std::string Call::to_string() const {
    auto params_to_string = [&] {
        std::ostringstream oss;
        for (size_t i = 0; i < get_params().size(); ++i) {
            const auto param = get_params()[i];
            oss << param->get_type()->to_string() << " " << param->get_name();
            if (i != get_params().size() - 1) oss << ", ";
        }
        return oss.str();
    };
    std::ostringstream oss;
    if (const_string_index != -1) {
        if (get_function()->get_name() != "putf") { log_error("Unknown const string index"); }
        if (get_params().empty()) {
            oss << "call void @putf(i8* @.str_" << const_string_index << ")";
        } else {
            oss << "call void @putf(i8* @.str_" << const_string_index << ", " << params_to_string() << ")";
        }
    } else {
        if (get_function()->get_type()->is_void()) {
            oss << "call " << get_function()->get_type()->to_string() << " @" << get_function()->get_name() << "(";
            oss << params_to_string() << ")";
        } else {
            oss << name_ << " = call " << get_function()->get_type()->to_string()
                    << " @" << get_function()->get_name() << "(";
            oss << params_to_string() << ")";
        }
    }
    return oss.str();
}


#define BINARY_TO_STRING(op_name, instr_name) \
[[nodiscard]] std::string op_name::to_string() const { \
    std::ostringstream oss; \
    oss << name_ << " = "; \
    oss << #instr_name << " " << get_lhs()->get_type()->to_string() << " " << get_lhs()->get_name() << ", " << get_rhs()->get_name(); \
    return oss.str(); \
}

BINARY_TO_STRING(Add, add)
BINARY_TO_STRING(Sub, sub)
BINARY_TO_STRING(Mul, mul)
BINARY_TO_STRING(Div, sdiv)
BINARY_TO_STRING(Mod, srem)
BINARY_TO_STRING(FAdd, fadd)
BINARY_TO_STRING(FSub, fsub)
BINARY_TO_STRING(FMul, fmul)
BINARY_TO_STRING(FDiv, fdiv)
BINARY_TO_STRING(FMod, frem)

#undef BINARY_TO_STRING

[[nodiscard]] std::string Phi::to_string() const {
    std::ostringstream oss;
    oss << name_ << " = phi " << type_->to_string();
    size_t i{0};
    for (const auto &[value, block]: optional_values) {
        oss << " [ " << block->get_name() << ", %" << value->get_name();
        if (i != optional_values.size() - 1) {
            oss << " ], ";
        } else {
            oss << " ]";
        }
        ++i;
    }
    return oss.str();
}

std::string Select::to_string() const {
    std::ostringstream oss;
    oss << name_ << " = select " << get_cond()->get_type()->to_string() << " " << get_cond()->get_name() << ", ";
    oss << get_true_value()->get_type()->to_string() << " " << get_true_value()->get_name() << ", "
            << get_false_value()->get_type()->to_string() << " " << get_false_value()->get_name();
    return oss.str();
}


namespace Init {
    [[nodiscard]] std::string Constant::to_string() const {
        return type->to_string() + " " + const_value->to_string();
    }

    [[nodiscard]] std::string Array::to_string() const {
        std::ostringstream oss;
        if (is_zero_initialized) {
            oss << type->to_string() << " zeroinitializer";
            return oss.str();
        }
        oss << type->to_string() << " [";
        for (size_t i = 0; i < init_values.size(); ++i) {
            oss << init_values[i]->to_string();
            if (i != init_values.size() - 1) {
                oss << ", ";
            }
        }
        oss << "]";
        return oss.str();
    }
}
}
