#include "Backend/InstructionSets/RISC-V/Modules.h"
#include "Backend/LIR/Instructions.h"
#include "Utils/Log.h"

RISCV::Module::Module(const std::shared_ptr<Backend::LIR::Module>& lir_module, const RegisterAllocator::AllocationType& allocation_type) {
    data_section = std::move(lir_module->global_data);
    for (const std::shared_ptr<Backend::LIR::Function>& mir_function : lir_module->functions) {
        if (mir_function->function_type == Backend::LIR::FunctionType::PRIVILEGED)
            continue;
        functions.push_back(std::make_shared<RISCV::Function>(mir_function, allocation_type));
        functions.back()->module = this;
    }
}

std::string RISCV::Module::to_string(const std::shared_ptr<Backend::DataSection> &data_section) {
    std::ostringstream oss;
    oss << ".section .rodata\n"
        << ".align 2\n";
    for (const std::pair<std::string, std::shared_ptr<Backend::DataSection::Variable>> var : data_section->global_variables)
        if (var.second->read_only) {
            // only str can trigger this
            oss << "str." << var.second->name << ":\n";
            oss << "  .string \"" << std::static_pointer_cast<Backend::DataSection::Variable::ConstString>(var.second->init_value)->str << "\"";
            oss << "\n";
        }
    oss << ".section .data\n"
        << ".align 2\n";
    for (const std::pair<std::string, std::shared_ptr<Backend::DataSection::Variable>> var : data_section->global_variables)
        if (!var.second->read_only) {
            // only int & float can trigger this
            oss << var.second->label() << ":\n";
            const std::vector<std::shared_ptr<Backend::Constant>> &constants = std::static_pointer_cast<Backend::DataSection::Variable::Constants>(var.second->init_value)->constants;
            for (const std::shared_ptr<Backend::Constant> &value: constants)
                oss << "  " << Backend::Utils::to_riscv_indicator(value->constant_type) << " " << value->name << "\n";
            if (constants.size() < var.second->length)
                oss << "  .zero " << (var.second->length - constants.size()) * Backend::Utils::type_to_size(var.second->workload_type) << "\n";
        }
    oss << "# END OF DATA FIELD\n";
    return oss.str();
}

RISCV::Function::Function(const std::shared_ptr<Backend::LIR::Function> &lir_function, const RegisterAllocator::AllocationType& allocation_type) : name(lir_function->name), lir_function(lir_function) {
    stack = std::make_shared<RISCV::Stack>();
    register_allocator = RISCV::RegisterAllocator::create(allocation_type, lir_function, stack);
    register_allocator->allocate();
}

void RISCV::Function::generate_prologue() {
    std::shared_ptr<RISCV::Block> block_entry = blocks.front();
    block_entry->instructions.insert(block_entry->instructions.begin(), std::make_shared<Instructions::AllocStack>(stack));
    if (lir_function->is_caller)
        block_entry->instructions.push_back(std::make_shared<Instructions::StoreRA>(stack));
}

template<typename T_instr, typename T_imm, typename T_reg>
void RISCV::Function::translate_iactions(const std::shared_ptr<T_instr> &instr, std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> &instrs) {
    std::shared_ptr<Backend::Operand> lhs = instr->lhs;
    std::shared_ptr<Backend::Operand> rhs = instr->rhs;
    std::shared_ptr<Backend::Variable> result = instr->result;
    RISCV::Registers::ABI rd = register_allocator->get_register(result);
    if (lhs->operand_type == Backend::OperandType::CONSTANT) {
        RISCV::Registers::ABI rs = register_allocator->get_register(std::static_pointer_cast<Backend::Variable>(rhs));
        instrs.push_back(std::make_shared<T_imm>(rd, rs, std::static_pointer_cast<Backend::IntValue>(lhs)->int32_value));
    } else if (rhs->operand_type == Backend::OperandType::CONSTANT) {
        RISCV::Registers::ABI rs = register_allocator->get_register(std::static_pointer_cast<Backend::Variable>(lhs));
        instrs.push_back(std::make_shared<T_imm>(rd, rs, std::static_pointer_cast<Backend::IntValue>(rhs)->int32_value));
    } else {
        RISCV::Registers::ABI rs1 = register_allocator->get_register(std::static_pointer_cast<Backend::Variable>(lhs));
        RISCV::Registers::ABI rs2 = register_allocator->get_register(std::static_pointer_cast<Backend::Variable>(rhs));
        instrs.push_back(std::make_shared<T_reg>(rd, rs1, rs2));
    }
}

template<typename T_instr>
void RISCV::Function::translate_bactions(const std::shared_ptr<Backend::LIR::BranchInstruction> &instr, std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> &instrs) {
    std::shared_ptr<RISCV::Block> target_block = find_block(instr->target_block->name);
    RISCV::Registers::ABI rs1 = register_allocator->get_register(instr->lhs);
    if (!instr->rhs) {
        instrs.push_back(std::make_shared<T_instr>(rs1, RISCV::Registers::ABI::ZERO, target_block));
    } else {
        RISCV::Registers::ABI rs2 = register_allocator->get_register(instr->rhs);
        instrs.push_back(std::make_shared<T_instr>(rs1, rs2, target_block));
    }
}

std::string RISCV::Block::label_name() const {
    return function.lock()->name + "_" + name;
}

std::string RISCV::Block::to_string() const {
    std::ostringstream oss;
    oss << " " << label_name() << ":\n";
    for (const std::shared_ptr<Instructions::Instruction>& instr : instructions) {
        oss << "  " << instr->to_string() << "\n";
    }
    return oss.str();
}

std::string RISCV::Function::to_string() const {
    std::ostringstream oss;
    oss << name << ":\n";
    for (const std::shared_ptr<RISCV::Block> &block: blocks)
        oss << block->to_string();
    return oss.str();
}

std::string RISCV::Module::to_string() const {
    std::ostringstream oss;
    oss << to_string(data_section) << "\n";
    oss << TEXT_OPTION << "\n";
    for (const std::shared_ptr<RISCV::Function> &function : functions) {
        oss << function->to_string() << "\n";
    }
    return oss.str();
}

void RISCV::Function::translate_blocks() {
    for (const std::shared_ptr<Backend::LIR::Block> &block : lir_function->blocks) {
        blocks.push_back(std::make_shared<RISCV::Block>(block->name, shared_from_this()));
    }
    for (const std::shared_ptr<Backend::LIR::Block> &mir_block : lir_function->blocks) {
        std::shared_ptr<RISCV::Block> block = find_block(mir_block->name);
        for (const std::shared_ptr<Backend::LIR::Instruction> &instr: mir_block->instructions) {
            std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> instructions = translate_instruction(instr);
            block->instructions.insert(block->instructions.end(), instructions.begin(), instructions.end());
        }
    }
}

std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> RISCV::Function::translate_instruction(const std::shared_ptr<Backend::LIR::Instruction>& instruction) {
    std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> instrs;
    switch (instruction->type) {
        case Backend::LIR::InstructionType::ADD: {
            std::shared_ptr<Backend::LIR::IntArithmetic> instr = std::static_pointer_cast<Backend::LIR::IntArithmetic>(instruction);
            translate_iactions<Backend::LIR::IntArithmetic, RISCV::Instructions::AddImmediate, RISCV::Instructions::Add>(instr, instrs);
            break;
        }
        case Backend::LIR::InstructionType::SUB: {
            std::shared_ptr<Backend::LIR::IntArithmetic> instr = std::static_pointer_cast<Backend::LIR::IntArithmetic>(instruction);
            translate_iactions<Backend::LIR::IntArithmetic, RISCV::Instructions::SubImmediate, RISCV::Instructions::Sub>(instr, instrs);
            break;
        }
        case Backend::LIR::InstructionType::MUL: {
            std::shared_ptr<Backend::LIR::IntArithmetic> instr = std::static_pointer_cast<Backend::LIR::IntArithmetic>(instruction);
            RISCV::Registers::ABI rs1 = register_allocator->get_register(instr->lhs);
            RISCV::Registers::ABI rs2 = register_allocator->get_register(std::static_pointer_cast<Backend::Variable>(instr->rhs));
            RISCV::Registers::ABI rd = register_allocator->get_register(instr->result);
            instrs.push_back(std::make_shared<RISCV::Instructions::Mul>(rd, rs1, rs2));
            break;
        }
        case Backend::LIR::InstructionType::DIV: {
            std::shared_ptr<Backend::LIR::IntArithmetic> instr = std::static_pointer_cast<Backend::LIR::IntArithmetic>(instruction);
            RISCV::Registers::ABI rs1 = register_allocator->get_register(instr->lhs);
            RISCV::Registers::ABI rs2 = register_allocator->get_register(std::static_pointer_cast<Backend::Variable>(instr->rhs));
            RISCV::Registers::ABI rd = register_allocator->get_register(instr->result);
            instrs.push_back(std::make_shared<RISCV::Instructions::Div>(rd, rs1, rs2));
            break;
        }
        case Backend::LIR::InstructionType::MOD: {
            std::shared_ptr<Backend::LIR::IntArithmetic> instr = std::static_pointer_cast<Backend::LIR::IntArithmetic>(instruction);
            RISCV::Registers::ABI rs1 = register_allocator->get_register(instr->lhs);
            RISCV::Registers::ABI rs2 = register_allocator->get_register(std::static_pointer_cast<Backend::Variable>(instr->rhs));
            RISCV::Registers::ABI rd = register_allocator->get_register(instr->result);
            instrs.push_back(std::make_shared<RISCV::Instructions::Mod>(rd, rs1, rs2));
            break;
        }
        case Backend::LIR::InstructionType::FADD: {
            // std::shared_ptr<Backend::LIR::FloatArithmeticInstruction> instr = std::static_pointer_cast<Backend::LIR::FloatArithmeticInstruction>(instruction);
            // RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->lhs), instrs);
            // RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->rhs), instrs);
            // RISCV::Registers::ABI rd = register_allocator->get_register(instr->result, instrs);
            break;
        }
        case Backend::LIR::InstructionType::FSUB: {
            // std::shared_ptr<Backend::LIR::FloatArithmeticInstruction> instr = std::static_pointer_cast<Backend::LIR::FloatArithmeticInstruction>(instruction);
            // RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->lhs), instrs);
            // RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->rhs), instrs);
            // RISCV::Registers::ABI rd = register_allocator->get_register(instr->result, instrs);
            break;
        }
        case Backend::LIR::InstructionType::FMUL: {
            // std::shared_ptr<Backend::LIR::FloatArithmeticInstruction> instr = std::static_pointer_cast<Backend::LIR::FloatArithmeticInstruction>(instruction);
            // RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->lhs), instrs);
            // RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->rhs), instrs);
            // RISCV::Registers::ABI rd = register_allocator->get_register(instr->result, instrs);
            break;
        }
        case Backend::LIR::InstructionType::FDIV: {
            // std::shared_ptr<Backend::LIR::FloatArithmeticInstruction> instr = std::static_pointer_cast<Backend::LIR::FloatArithmeticInstruction>(instruction);
            // RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->lhs), instrs);
            // RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->rhs), instrs);
            // RISCV::Registers::ABI rd = register_allocator->get_register(instr->result, instrs);
            break;
        }
        case Backend::LIR::InstructionType::LOAD_IMM: {
            std::shared_ptr<Backend::LIR::LoadIntImm> instr = std::static_pointer_cast<Backend::LIR::LoadIntImm>(instruction);
            RISCV::Registers::ABI rd = register_allocator->get_register(instr->var_in_reg);
            instrs.push_back(std::make_shared<RISCV::Instructions::LoadImmediate>(rd, instr->immediate->int32_value));
            break;
        }
        case Backend::LIR::InstructionType::LOAD_ADDR: {
            std::shared_ptr<Backend::LIR::LoadAddress> instr = std::static_pointer_cast<Backend::LIR::LoadAddress>(instruction);
            RISCV::Registers::ABI rd = register_allocator->get_register(instr->addr);
            instrs.push_back(std::make_shared<RISCV::Instructions::LoadAddress>(rd, instr->var_in_mem));
            break;
        }
        case Backend::LIR::InstructionType::MOVE: {
            std::shared_ptr<Backend::LIR::Move> instr = std::static_pointer_cast<Backend::LIR::Move>(instruction);
            RISCV::Registers::ABI rd = register_allocator->get_register(instr->target);
            RISCV::Registers::ABI rs = register_allocator->get_register(instr->source);
            if (rd != rs)
                instrs.push_back(std::make_shared<RISCV::Instructions::Add>(rd, RISCV::Registers::ABI::ZERO, rs));
            break;
        }
        case Backend::LIR::InstructionType::LOAD: {
            std::shared_ptr<Backend::LIR::LoadInt> instr = std::static_pointer_cast<Backend::LIR::LoadInt>(instruction);
            std::shared_ptr<Backend::Variable> addr = instr->var_in_mem;
            std::shared_ptr<Backend::Variable> dest = instr->var_in_reg;
            RISCV::Registers::ABI dest_reg = register_allocator->get_register(dest);
            if (addr->lifetime == Backend::VariableWide::GLOBAL) {
                instrs.push_back(std::make_shared<RISCV::Instructions::LoadAddress>(dest_reg, addr));
                instrs.push_back(std::make_shared<RISCV::Instructions::LoadWord>(dest_reg, dest_reg, instr->offset));
            } else {
                instrs.push_back(std::make_shared<RISCV::Instructions::LoadWordFromStack>(dest_reg, addr, stack));
            }
            break;
        }
        case Backend::LIR::InstructionType::STORE: {
            std::shared_ptr<Backend::LIR::StoreInt> instr = std::static_pointer_cast<Backend::LIR::StoreInt>(instruction);
            std::shared_ptr<Backend::Variable> dest = instr->var_in_mem;
            std::shared_ptr<Backend::Variable> src = instr->var_in_reg;
            RISCV::Registers::ABI src_reg = register_allocator->get_register(src);
            if (dest->lifetime == Backend::VariableWide::FUNCTIONAL)
                instrs.push_back(std::make_shared<RISCV::Instructions::StoreWordToStack>(src_reg, dest, stack));
            else
                instrs.push_back(std::make_shared<RISCV::Instructions::StoreWord>(register_allocator->get_register(dest), src_reg, instr->offset));
            break;
        }
        case Backend::LIR::InstructionType::CALL: {
            std::shared_ptr<Backend::LIR::Call> instr = std::static_pointer_cast<Backend::LIR::Call>(instruction);
            instrs.push_back(std::make_shared<RISCV::Instructions::Call>(instr->function->name));
            break;
        }
        case Backend::LIR::InstructionType::JUMP: {
            std::shared_ptr<Backend::LIR::Jump> instr = std::static_pointer_cast<Backend::LIR::Jump>(instruction);
            std::string target_block_name = instr->target_block->name;
            std::shared_ptr<RISCV::Block> target_block = find_block(target_block_name);
            instrs.push_back(std::make_shared<RISCV::Instructions::Jump>(target_block));
            break;
        }
        case Backend::LIR::InstructionType::EQUAL:
        case Backend::LIR::InstructionType::EQUAL_ZERO: {
            translate_bactions<RISCV::Instructions::BranchOnEqual>(std::static_pointer_cast<Backend::LIR::BranchInstruction>(instruction), instrs);
            break;
        }
        case Backend::LIR::InstructionType::NOT_EQUAL:
        case Backend::LIR::InstructionType::NOT_EQUAL_ZERO: {
            translate_bactions<RISCV::Instructions::BranchOnNotEqual>(std::static_pointer_cast<Backend::LIR::BranchInstruction>(instruction), instrs);
            break;
        }
        case Backend::LIR::InstructionType::GREATER:
        case Backend::LIR::InstructionType::GREATER_ZERO: {
            translate_bactions<RISCV::Instructions::BranchOnGreaterThan>(std::static_pointer_cast<Backend::LIR::BranchInstruction>(instruction), instrs);
            break;
        }
        case Backend::LIR::InstructionType::LESS:
        case Backend::LIR::InstructionType::LESS_ZERO: {
            translate_bactions<RISCV::Instructions::BranchOnLessThan>(std::static_pointer_cast<Backend::LIR::BranchInstruction>(instruction), instrs);
            break;
        }
        case Backend::LIR::InstructionType::GREATER_EQUAL:
        case Backend::LIR::InstructionType::GREATER_EQUAL_ZERO: {
            translate_bactions<RISCV::Instructions::BranchOnGreaterThanOrEqual>(std::static_pointer_cast<Backend::LIR::BranchInstruction>(instruction), instrs);
            break;
        }
        case Backend::LIR::InstructionType::LESS_EQUAL:
        case Backend::LIR::InstructionType::LESS_EQUAL_ZERO: {
            translate_bactions<RISCV::Instructions::BranchOnLessThanOrEqual>(std::static_pointer_cast<Backend::LIR::BranchInstruction>(instruction), instrs);
            break;
        }
        case Backend::LIR::InstructionType::RETURN: {
            std::shared_ptr<Backend::LIR::Return> instr = std::static_pointer_cast<Backend::LIR::Return>(instruction);
            if (lir_function->is_caller)
                instrs.push_back(std::make_shared<RISCV::Instructions::LoadRA>(stack));
            instrs.push_back(std::make_shared<RISCV::Instructions::FreeStack>(stack));
            instrs.push_back(std::make_shared<RISCV::Instructions::Ret>());
            break;
        }
        default: break;
    }
    return instrs;
}