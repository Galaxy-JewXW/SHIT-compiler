#include "Backend/Instructions/RISC-V/Modules.h"
#include "Backend/Instructions/RISC-V/Variables.h"
#include "Backend/Instructions/RISC-V/Registers.h"
#include "Mir/Init.h"

[[nodiscard]] std::string RISCV::Modules::TextField::to_string() const {
    std::ostringstream oss;
    oss << ".section .text\n.global main\n";
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
                // return std::make_shared<RISCV::Instructions::Load>(load->get_name(), load->get_addr());
                break;
            }
            case Mir::Operator::STORE: {
                std::shared_ptr<Mir::Store> store = std::dynamic_pointer_cast<Mir::Store>(instruction);
                std::string addr = store->get_addr()->get_name();
                std::shared_ptr<Mir::Value> value = store->get_value();
                if (function_field.memory.vreg2offset.find(addr) != function_field.memory.vreg2offset.end()) {
                    size_t offset = function_field.sp - function_field.memory.vreg2offset[addr];
                    if (value->is_constant()) {
                        function_field.add_instruction(std::make_shared<RISCV::Instructions::Addi>(RISCV::Instructions::Register(RISCV::Instructions::Registers::A0), RISCV::Instructions::Register(RISCV::Instructions::Registers::ZERO), RISCV::Instructions::Immediate(value->get_name())));
                        function_field.add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(RISCV::Instructions::Register(RISCV::Instructions::Registers::A0), RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Immediate(offset)));
                    }
                } else {
                    function_field.add_instruction(std::make_shared<RISCV::Instructions::LoadAddress>(RISCV::Instructions::Register(RISCV::Instructions::Registers::A0), addr));
                    function_field.add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(RISCV::Instructions::Register(RISCV::Instructions::Registers::A0), RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Immediate(0)));
                }
                break;
            }
            case Mir::Operator::GEP: {
            }
            case Mir::Operator::BITCAST: {
            }
            case Mir::Operator::FPTOSI: {
            }
            case Mir::Operator::FCMP: {
            }
            case Mir::Operator::ICMP: {
            }
            case Mir::Operator::ZEXT: {
            }
            case Mir::Operator::BRANCH: {
            }
            case Mir::Operator::JUMP: {
            }
            case Mir::Operator::RET: {
                break;
            }
            case Mir::Operator::CALL: {
                std::shared_ptr<Mir::Call> call = std::dynamic_pointer_cast<Mir::Call>(instruction);
                std::string function_name = call->get_function()->get_name();
                if (function_name == "putf" && call->get_const_string_index() >= 0) {
                    function_field.add_instruction(std::make_shared<RISCV::Instructions::Addi>(RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Immediate(-16)));
                    function_field.add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(RISCV::Instructions::Register(RISCV::Instructions::Registers::RA), RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Immediate(8)));
                    function_field.add_instruction(std::make_shared<RISCV::Instructions::LoadAddress>(RISCV::Instructions::Register(RISCV::Instructions::Registers::A0), ".str_" + std::to_string(call->get_const_string_index())));
                    function_field.add_instruction(std::make_shared<RISCV::Instructions::Call>("putf"));
                    function_field.add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Instructions::Register(RISCV::Instructions::Registers::RA), RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Immediate(8)));
                    function_field.add_instruction(std::make_shared<RISCV::Instructions::Addi>(RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Immediate(16)));
                    break;
                }
                std::vector<std::shared_ptr<Mir::Value>> params = call->get_params();
                function_field.add_instruction(std::make_shared<RISCV::Instructions::Addi>(RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Immediate(-((params.size() + 1) << 3))));
                function_field.add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(RISCV::Instructions::Register(RISCV::Instructions::Registers::RA), RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Immediate(params.size() << 3)));
                for (long long unsigned int i = 0; i < params.size(); i++) {
                    std::shared_ptr<Mir::Value> param = params[i];
                    if (param->is_constant()) {
                        function_field.add_instruction(std::make_shared<RISCV::Instructions::Addi>(RISCV::Instructions::Register(RISCV::Instructions::Registers::A0), RISCV::Instructions::Register(RISCV::Instructions::Registers::ZERO), RISCV::Instructions::Immediate(param->get_name())));
                    } else if (function_field.memory.vreg2offset.find(param->get_name()) != function_field.memory.vreg2offset.end()) {
                        size_t offset = function_field.sp - function_field.memory.vreg2offset[param->get_name()];
                        function_field.add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Instructions::Register(RISCV::Instructions::Registers::A0), RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Immediate(offset)));
                    } else {
                        function_field.add_instruction(std::make_shared<RISCV::Instructions::LoadAddress>(RISCV::Instructions::Register(RISCV::Instructions::Registers::A0), param->get_name()));
                        function_field.add_instruction(std::make_shared<RISCV::Instructions::StoreDoubleword>(RISCV::Instructions::Register(RISCV::Instructions::Registers::A0), RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Immediate(0)));
                    }
                }
                function_field.add_instruction(std::make_shared<RISCV::Instructions::Call>(function_name));
                function_field.add_instruction(std::make_shared<RISCV::Instructions::LoadDoubleword>(RISCV::Instructions::Register(RISCV::Instructions::Registers::RA), RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Immediate(params.size() << 3)));
                function_field.add_instruction(std::make_shared<RISCV::Instructions::Addi>(RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Register(RISCV::Instructions::Registers::SP), RISCV::Instructions::Immediate((params.size() + 1) << 3)));
                break;
            }
            case Mir::Operator::INTBINARY: {
            }
            case Mir::Operator::FLOATBINARY: {
            }
            default: break;
        }
    }

    void alloc_all(const std::shared_ptr<Mir::Function> &function, RISCV::Modules::FunctionField &function_field) {
        for (const std::shared_ptr<Mir::Block> &block: function->get_blocks()) {
            for (const std::shared_ptr<Mir::Instruction> &instruction: block->get_instructions()) {
                if (instruction->get_op() == Mir::Operator::ALLOC) {
                    std::shared_ptr<Mir::Alloc> alloc = std::dynamic_pointer_cast<Mir::Alloc>(instruction);
                    std::string vreg = alloc->get_name();
                    std::shared_ptr<Mir::Type::Type> type_ = alloc->get_type();
                    size_t size = RISCV::Variables::VariableTypeUtils::type_to_size(RISCV::Variables::VariableTypeUtils::llvm_to_riscv(*type_));
                    function_field.memory.vreg2offset[vreg] = function_field.sp += size;
                    log_debug("Alloc: %s, size: %zu, $sp: %zu", vreg.c_str(), size, function_field.sp);
                }
            }
        }
    }
}