#include "Backend/Instructions/RISC-V/Modules.h"
#include "Backend/Instructions/RISC-V/Variables.h"
#include "Backend/Instructions/RISC-V/Registers.h"
#include "Mir/Init.h"
#include "Utils/Log.h"

[[nodiscard]] std::string RISCV::Modules::TextField::to_string() const {
    std::ostringstream oss;
    oss << ".section .text\n.option norvc\n.global main\n";
    for (const std::shared_ptr<RISCV::Modules::FunctionField> &function : this->functions) {
        oss << function->to_string() << "\n";
    }
    return oss.str();
}

void RISCV::Modules::TextField::add_function(const FunctionField &function) {
    this->functions.push_back(std::make_shared<RISCV::Modules::FunctionField>(function));
}

[[nodiscard]] std::string RISCV::Modules::DataField::to_string() const {
    std::ostringstream oss;
    oss << ".section .rodata\n";
    size_t len = this->const_strings.size();
    for (size_t i = 1; i <= len; i++) {
        oss << ".str_" << i << ": .string \"" << this->const_strings[i - 1] << "\"\n";
    }
    oss << ".section .data\n";
    for (const std::shared_ptr<RISCV::Variables::Variable> &var : this->global_variables) {
        oss << var->to_string() << "\n";
    }
    oss << "# END OF DATA FIELD\n";
    return oss.str();
}

void RISCV::Modules::DataField::load_global_variables(const std::vector<std::shared_ptr<Mir::GlobalVariable>> &global_variables) {
    for (const std::shared_ptr<Mir::GlobalVariable> &global_variable : global_variables) {
        Mir::Init::Init *init_value = global_variable->get_init_value().get();
        RISCV::Variables::VariableType type = RISCV::Variables::VariableTypeUtils::llvm_to_riscv(*init_value->get_type());
        if (type == RISCV::Variables::VariableType::ARRAY) {
            const Mir::Type::Array *array = dynamic_cast<const Mir::Type::Array *>(init_value->get_type().get());
            std::shared_ptr<Mir::Type::Type> _type = array->get_element_type();
            RISCV::Variables::VariableType element_type = RISCV::Variables::VariableTypeUtils::llvm_to_riscv(*_type);
            size_t array_size = array->get_size();
            RISCV::Variables::Array array_var(global_variable->get_name(), element_type, RISCV::Variables::VariableInitValueUtils::load_from_llvm(*dynamic_cast<const Mir::Init::Array *>(init_value), element_type, array_size), array_size);
            this->global_variables.push_back(std::make_shared<RISCV::Variables::Array>(array_var));
        } else {
            RISCV::Variables::Variable var(global_variable->get_name(), type, RISCV::Variables::VariableInitValueUtils::load_from_llvm(*init_value, type));
            this->global_variables.push_back(std::make_shared<RISCV::Variables::Variable>(var));
        }
    }
}

[[nodiscard]] std::string RISCV::Modules::FunctionField::to_string() const {
    std::ostringstream oss;
    oss << this->function_name << ":\n";
    for (const std::shared_ptr<RISCV::Instructions::Instruction> &instruction : this->instructions) {
        oss << "  " << instruction->to_string() << "\n";
    }
    return oss.str();
}

void RISCV::Modules::FunctionField::add_instruction(const std::shared_ptr<RISCV::Instructions::Instruction> &instruction) {
    this->instructions.push_back(instruction);
}

namespace RISCV::Instructions::InstructionFactory {
    void create(const std::shared_ptr<Mir::Instruction> &instruction, RISCV::Modules::FunctionField &function_field) {
        switch (instruction->get_op()) {
            case Mir::Operator::LOAD: {
                std::shared_ptr<Mir::Load> load = std::dynamic_pointer_cast<Mir::Load>(instruction);
                std::string load_from = load->get_addr()->get_name();
                std::string load_to = instruction->get_name();
                function_field.memory->load_to(load_from, load_to);
                break;
            }
            case Mir::Operator::STORE: {
                std::shared_ptr<Mir::Store> store = std::dynamic_pointer_cast<Mir::Store>(instruction);
                std::string store_to = store->get_addr()->get_name();
                std::shared_ptr<Mir::Value> value = store->get_value();
                function_field.memory->store_to(store_to, value);
                break;
            }
            case Mir::Operator::GEP: {
                std::shared_ptr<Mir::GetElementPtr> get_element_pointer = std::dynamic_pointer_cast<Mir::GetElementPtr>(instruction);
                std::string array = get_element_pointer->get_addr()->get_name();
                std::shared_ptr<Mir::Value> offset = get_element_pointer->get_index();
                std::string store_to = instruction->get_name();
                function_field.memory->get_element_pointer(array, offset, store_to);
                break;
            }
            case Mir::Operator::BITCAST: {
                break;
            }
            case Mir::Operator::FPTOSI: {
                break;
            }
            case Mir::Operator::FCMP: {
                break;
            }
            case Mir::Operator::ICMP: {
                std::shared_ptr<Mir::Icmp> icmp = std::dynamic_pointer_cast<Mir::Icmp>(instruction);
                switch (icmp->op) {
                    case Mir::Icmp::Op::EQ: {
                        transform_integer_binary<RISCV::Instructions::BranchOnEqual>(icmp, function_field);
                        break;
                    }
                    case Mir::Icmp::Op::NE: {
                        transform_integer_binary<RISCV::Instructions::BranchOnNotEqual>(icmp, function_field);
                        break;
                    }
                    case Mir::Icmp::Op::GT: {
                        transform_integer_binary<RISCV::Instructions::BranchOnGreaterThan>(icmp, function_field);
                        break;
                    }
                    case Mir::Icmp::Op::LT: {
                        transform_integer_binary<RISCV::Instructions::BranchOnLessThan>(icmp, function_field);
                        break;
                    }
                    case Mir::Icmp::Op::GE: {
                        transform_integer_binary<RISCV::Instructions::BranchOnGreaterThanOrEqual>(icmp, function_field);
                        break;
                    }
                    case Mir::Icmp::Op::LE: {
                        transform_integer_binary<RISCV::Instructions::BranchOnLessThanOrEqual>(icmp, function_field);
                        break;
                    }
                    default: break;
                }
                break;
            }
            case Mir::Operator::ZEXT: {
                break;
            }
            case Mir::Operator::BRANCH: {
                std::shared_ptr<Mir::Branch> branch = std::dynamic_pointer_cast<Mir::Branch>(instruction);
                std::shared_ptr<Mir::Value> cond = branch->get_cond();
                std::string label_true = "." + function_field.function_name + "." + branch->get_true_block()->get_name();
                std::string label_false = "." + function_field.function_name + "." + branch->get_false_block()->get_name();
                function_field.memory->load_to(RISCV::Registers::A0, cond);
                function_field.add_instruction(std::make_shared<RISCV::Instructions::BranchOnEqual>(RISCV::Registers::A0, RISCV::Registers::ZERO, label_false));
                function_field.add_instruction(std::make_shared<RISCV::Instructions::Jump>(label_true));
                break;
            }
            case Mir::Operator::JUMP: {
                std::shared_ptr<Mir::Jump> jump = std::dynamic_pointer_cast<Mir::Jump>(instruction);
                std::string label = "." + function_field.function_name + "." + jump->get_target_block()->get_name();
                function_field.add_instruction(std::make_shared<RISCV::Instructions::Jump>(label));
                break;
            }
            case Mir::Operator::RET: {
                std::shared_ptr<Mir::Ret> ret = std::dynamic_pointer_cast<Mir::Ret>(instruction);
                std::shared_ptr<Mir::Value> value = ret->get_value();
                if (value->is_constant()) {
                    function_field.add_instruction(std::make_shared<RISCV::Instructions::LoadImmediate>(RISCV::Registers::A0, value->get_name()));
                }
                function_field.add_instruction(std::make_shared<RISCV::Instructions::AddImmediate>(function_field.sp, function_field.sp, function_field.sp->offset));
                function_field.add_instruction(std::make_shared<RISCV::Instructions::Ret>());
                break;
            }
            case Mir::Operator::CALL: {
                std::shared_ptr<Mir::Call> call = std::dynamic_pointer_cast<Mir::Call>(instruction);
                std::string function_name = call->get_function()->get_name();
                if (function_name == "putf" && call->get_const_string_index() >= 0) {
                    function_field.add_instruction(std::make_shared<RISCV::Instructions::AddImmediate>(function_field.sp, function_field.sp, -16));
                    function_field.add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(RISCV::Registers::RA, function_field.sp, 8));
                    function_field.add_instruction(std::make_shared<RISCV::Instructions::LoadAddress>(RISCV::Registers::A0, ".str_" + std::to_string(call->get_const_string_index())));
                    function_field.add_instruction(std::make_shared<RISCV::Instructions::Call>("putf"));
                    function_field.add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::RA, function_field.sp, 8));
                    function_field.add_instruction(std::make_shared<RISCV::Instructions::AddImmediate>(function_field.sp, function_field.sp, 16));
                    break;
                }
                std::vector<std::shared_ptr<Mir::Value>> params = call->get_params();
                function_field.sp->alloc_stack((params.size() + 1) << 3);
                function_field.add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(RISCV::Registers::RA, function_field.sp, params.size() << 3));
                for (size_t i = 0; i < params.size(); i++) {
                    std::shared_ptr<Mir::Value> param = params[i];
                    if (i < 4) {
                        function_field.memory->load_to(RISCV::Registers::A0 + i, param);
                    } else {
                        //
                    }
                }
                function_field.add_instruction(std::make_shared<RISCV::Instructions::Call>(function_name));
                function_field.add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Registers::RA, function_field.sp, params.size() << 3));
                function_field.sp->free_stack((size_t)1);
                break;
            }
            case Mir::Operator::INTBINARY: {
                std::shared_ptr<Mir::IntBinary> int_operation_ = std::dynamic_pointer_cast<Mir::IntBinary>(instruction);
                switch (int_operation_->op) {
                    case Mir::IntBinary::Op::ADD: {
                        transform_integer_binary<RISCV::Instructions::Add>(int_operation_, function_field);
                        break;
                    }
                    case Mir::IntBinary::Op::SUB: {
                        transform_integer_binary<RISCV::Instructions::Sub>(int_operation_, function_field);
                        break;
                    }
                    case Mir::IntBinary::Op::MUL: {
                        transform_integer_binary<RISCV::Instructions::Mul>(int_operation_, function_field);
                        break;
                    }
                    case Mir::IntBinary::Op::DIV: {
                        transform_integer_binary<RISCV::Instructions::Div>(int_operation_, function_field);
                        break;
                    }
                    case Mir::IntBinary::Op::MOD: {
                        transform_integer_binary<RISCV::Instructions::Mod>(int_operation_, function_field);
                        break;
                    }
                }
                break;
            }
            case Mir::Operator::FLOATBINARY: {
                std::shared_ptr<Mir::FloatBinary> float_operation_ = std::dynamic_pointer_cast<Mir::FloatBinary>(instruction);
                switch (float_operation_->op) {
                    case Mir::FloatBinary::Op::ADD: break;
                    case Mir::FloatBinary::Op::SUB: break;
                    case Mir::FloatBinary::Op::MUL: break;
                    case Mir::FloatBinary::Op::DIV: break;
                    case Mir::FloatBinary::Op::MOD: break;
                }
                break;
            }
            default: break;
        }
    }

    template <typename T>
    void transform_integer_binary(const std::shared_ptr<Mir::IntBinary> &instruction, RISCV::Modules::FunctionField &function_field) {
        std::shared_ptr<Mir::Value> lhs = instruction->get_lhs();
        std::shared_ptr<Mir::Value> rhs = instruction->get_rhs();
        std::string store_to = instruction->get_name();
        function_field.memory->load_to(RISCV::Registers::A0, lhs);
        function_field.memory->load_to(RISCV::Registers::A7, rhs);
        function_field.add_instruction(std::make_shared<T>(RISCV::Registers::A0, RISCV::Registers::A0, RISCV::Registers::A7));
        function_field.memory->store_to(store_to, RISCV::Registers::A0);
    }

    template <typename T>
    void transform_integer_binary(const std::shared_ptr<Mir::Icmp> &instruction, RISCV::Modules::FunctionField &function_field) {
        std::shared_ptr<Mir::Value> lhs = instruction->get_lhs();
        std::shared_ptr<Mir::Value> rhs = instruction->get_rhs();
        std::string store_to = instruction->get_name();
        function_field.memory->load_to(RISCV::Registers::A0, lhs);
        function_field.memory->load_to(RISCV::Registers::A7, rhs);
        std::string label = RISCV::Instructions::Label::temporary_label();
        function_field.add_instruction(std::make_shared<RISCV::Instructions::LoadImmediate>(RISCV::Registers::A1, 1));
        function_field.add_instruction(std::make_shared<T>(RISCV::Registers::A0, RISCV::Registers::A7, label));
        function_field.add_instruction(std::make_shared<RISCV::Instructions::LoadImmediate>(RISCV::Registers::A1, 0));
        function_field.add_instruction(std::make_shared<RISCV::Instructions::Label>(label));
        function_field.memory->store_to(store_to, RISCV::Registers::A1);
    }

    void alloc_all(const std::shared_ptr<Mir::Function> &function, RISCV::Modules::FunctionField &function_field) {
        for (const std::shared_ptr<Mir::Block> &block: function->get_blocks()) {
            for (const std::shared_ptr<Mir::Instruction> &instruction: block->get_instructions()) {
                if (instruction->get_op() == Mir::Operator::ALLOC) {
                    std::shared_ptr<Mir::Alloc> alloc = std::dynamic_pointer_cast<Mir::Alloc>(instruction);
                    std::string vreg = alloc->get_name();
                    std::shared_ptr<Mir::Type::Type> type_ = alloc->get_type();
                    size_t size = RISCV::Variables::VariableTypeUtils::type_to_size(RISCV::Variables::VariableTypeUtils::llvm_to_riscv(*type_));
                    function_field.memory->vptr2offset[vreg] = function_field.sp->offset += size;
                    log_debug("Alloc: %s, size: %zu, $sp: %zu", vreg.c_str(), size, function_field.sp->offset);
                } else if (std::string vreg = instruction->get_name(); !vreg.empty()) {
                    std::shared_ptr<Mir::Type::Type> type_ = instruction->get_type();
                    size_t size = RISCV::Variables::VariableTypeUtils::type_to_size(RISCV::Variables::VariableTypeUtils::llvm_to_riscv(*type_));
                    function_field.memory->vreg2offset[vreg] = function_field.sp->offset += size;
                    log_debug("Alloc: %s, size: %zu, $sp: %zu", vreg.c_str(), size, function_field.sp->offset);
                }
            }
        }
        function_field.sp->alloc_record.push_back(function_field.sp->offset);
    }
}