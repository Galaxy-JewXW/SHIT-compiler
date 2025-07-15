#include "Backend/InstructionSets/RISC-V/Modules.h"
#include "Backend/LIR/Instructions.h"
#include "Utils/Log.h"

RISCV::Module::Module(const std::shared_ptr<Backend::LIR::Module>& mir_module, const RegisterAllocator::AllocationType& allocation_type) {
    data_section = std::move(mir_module->global_data);
    for (const std::shared_ptr<Backend::LIR::Function>& mir_function : mir_module->functions) {
        if (mir_function->function_type == Backend::LIR::FunctionType::PRIVILEGED)
            continue;
        functions.push_back(std::make_shared<RISCV::Function>(mir_function, allocation_type));
        functions.back()->module = this;
    }
}

RISCV::Function::Function(const std::shared_ptr<Backend::LIR::Function> &mir_function, const RegisterAllocator::AllocationType& allocation_type) : name(mir_function->name), mir_function_(mir_function) {
    stack = std::make_shared<RISCV::Stack>();
    register_allocator = RISCV::RegisterAllocator::create(allocation_type, mir_function, stack);
    register_allocator->allocate();
}

void RISCV::Function::to_assembly() {
    generate_prologue();
    translate_blocks();
}

void RISCV::Function::generate_prologue() {
    std::shared_ptr<RISCV::Block> block_entry = std::make_shared<RISCV::Block>("block_entry", shared_from_this());
    blocks.push_back(block_entry);
    block_entry->instructions.push_back(std::make_shared<Instructions::AllocStack>(stack));
    block_entry->instructions.push_back(std::make_shared<Instructions::StoreRA>(stack));
}

template<typename T_instr, typename T_imm, typename T_reg>
void RISCV::Function::translate_iactions(std::shared_ptr<T_instr> &instr, std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> &instrs) {
    std::shared_ptr<Backend::LIR::Value> lhs = instr->lhs;
    std::shared_ptr<Backend::LIR::Value> rhs = instr->rhs;
    std::shared_ptr<Backend::Variable> result = instr->result;
    if (lhs->value_type == Backend::LIR::OperandType::CONSTANT && rhs->value_type == Backend::LIR::OperandType::CONSTANT) {
        log_error("Cannot calculate two constants in RISC-V backend");
    }
    RISCV::Registers::ABI rd = register_allocator->use_register_w(result, instrs);
    if (lhs->value_type == Backend::LIR::OperandType::CONSTANT) {
        RISCV::Registers::ABI rs = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(rhs), instrs);
        instrs.push_back(std::make_shared<T_imm>(rd, rs, std::static_pointer_cast<Backend::LIR::Constant>(lhs)->int32_value));
    } else if (rhs->value_type == Backend::LIR::OperandType::CONSTANT) {
        RISCV::Registers::ABI rs = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(lhs), instrs);
        instrs.push_back(std::make_shared<T_imm>(rd, rs, std::static_pointer_cast<Backend::LIR::Constant>(rhs)->int32_value));
    } else {
        RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(lhs), instrs);
        RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(rhs), instrs);
        instrs.push_back(std::make_shared<T_reg>(rd, rs1, rs2));
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
    oss << data_section->to_string() << "\n";
    oss << TEXT_OPTION << "\n";
    for (const std::shared_ptr<RISCV::Function> &function : functions) {
        oss << function->to_string() << "\n";
    }
    return oss.str();
}

void RISCV::Function::translate_blocks() {
    for (const std::shared_ptr<Backend::LIR::Block> &block : mir_function_->blocks) {
        blocks.push_back(std::make_shared<RISCV::Block>(block->name, shared_from_this()));
    }
    for (const std::shared_ptr<Backend::LIR::Block> &mir_block : mir_function_->blocks) {
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
            // std::shared_ptr<Backend::LIR::IntArithmetic> instr = std::static_pointer_cast<Backend::LIR::IntArithmetic>(instruction);
            // translate_iactions<Backend::LIR::IntArithmetic, RISCV::Instructions::AddImmediate, RISCV::Instructions::Add>(instr, instrs);
            break;
        }
        case Backend::LIR::InstructionType::SUB: {
            // std::shared_ptr<Backend::LIR::IntArithmetic> instr = std::static_pointer_cast<Backend::LIR::IntArithmetic>(instruction);
            // translate_iactions<Backend::LIR::IntArithmetic, RISCV::Instructions::SubImmediate, RISCV::Instructions::Sub>(instr, instrs);
            break;
        }
        case Backend::LIR::InstructionType::MUL: {
            // std::shared_ptr<Backend::LIR::IntArithmetic> instr = std::static_pointer_cast<Backend::LIR::IntArithmetic>(instruction);
            // RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->lhs), instrs);
            // RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->rhs), instrs);
            // RISCV::Registers::ABI rd = register_allocator->use_register_w(instr->result, instrs);
            // instrs.push_back(std::make_shared<RISCV::Instructions::Mul>(rd, rs1, rs2));
            break;
        }
        case Backend::LIR::InstructionType::DIV: {
            // std::shared_ptr<Backend::LIR::IntArithmetic> instr = std::static_pointer_cast<Backend::LIR::IntArithmetic>(instruction);
            // RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->lhs), instrs);
            // RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->rhs), instrs);
            // RISCV::Registers::ABI rd = register_allocator->use_register_w(instr->result, instrs);
            // instrs.push_back(std::make_shared<RISCV::Instructions::Div>(rd, rs1, rs2));
            break;
        }
        case Backend::LIR::InstructionType::MOD: {
            // std::shared_ptr<Backend::LIR::IntArithmetic> instr = std::static_pointer_cast<Backend::LIR::IntArithmetic>(instruction);
            // RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->lhs), instrs);
            // RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->rhs), instrs);
            // RISCV::Registers::ABI rd = register_allocator->use_register_w(instr->result, instrs);
            // instrs.push_back(std::make_shared<RISCV::Instructions::Mod>(rd, rs1, rs2));
            break;
        }
        case Backend::LIR::InstructionType::FADD: {
            // std::shared_ptr<Backend::LIR::FloatArithmeticInstruction> instr = std::static_pointer_cast<Backend::LIR::FloatArithmeticInstruction>(instruction);
            // RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->lhs), instrs);
            // RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->rhs), instrs);
            // RISCV::Registers::ABI rd = register_allocator->use_register_w(instr->result, instrs);
            break;
        }
        case Backend::LIR::InstructionType::FSUB: {
            // std::shared_ptr<Backend::LIR::FloatArithmeticInstruction> instr = std::static_pointer_cast<Backend::LIR::FloatArithmeticInstruction>(instruction);
            // RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->lhs), instrs);
            // RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->rhs), instrs);
            // RISCV::Registers::ABI rd = register_allocator->use_register_w(instr->result, instrs);
            break;
        }
        case Backend::LIR::InstructionType::FMUL: {
            // std::shared_ptr<Backend::LIR::FloatArithmeticInstruction> instr = std::static_pointer_cast<Backend::LIR::FloatArithmeticInstruction>(instruction);
            // RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->lhs), instrs);
            // RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->rhs), instrs);
            // RISCV::Registers::ABI rd = register_allocator->use_register_w(instr->result, instrs);
            break;
        }
        case Backend::LIR::InstructionType::FDIV: {
            // std::shared_ptr<Backend::LIR::FloatArithmeticInstruction> instr = std::static_pointer_cast<Backend::LIR::FloatArithmeticInstruction>(instruction);
            // RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->lhs), instrs);
            // RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->rhs), instrs);
            // RISCV::Registers::ABI rd = register_allocator->use_register_w(instr->result, instrs);
            break;
        }
        case Backend::LIR::InstructionType::LOAD_IMM: {
            // std::shared_ptr<Backend::LIR::LoadI32> instr = std::static_pointer_cast<Backend::LIR::LoadI32>(instruction);
            // RISCV::Registers::ABI rd = register_allocator->use_register_w(instr->result, instrs);
            // if (instr->immediate->constant_type == Backend::VariableType::FLOAT) {
            //     // 浮点立即数加载，暂时跳过
            // } else {
            //     instrs.push_back(std::make_shared<RISCV::Instructions::LoadImmediate>(rd, instr->immediate->int32_value));
            // }
            break;
        }
        case Backend::LIR::InstructionType::LOAD_ADDR: {
            // std::shared_ptr<Backend::LIR::LoadAddressInstruction> instr = std::static_pointer_cast<Backend::LIR::LoadAddressInstruction>(instruction);
            // RISCV::Registers::ABI rd = register_allocator->use_register_w(instr->result, instrs);
            // instrs.push_back(std::make_shared<RISCV::Instructions::LoadAddress>(rd, instr->address_variable));
            break;
        }
        case Backend::LIR::InstructionType::MOVE: {
            // std::shared_ptr<Backend::LIR::MoveInstruction> instr = std::static_pointer_cast<Backend::LIR::MoveInstruction>(instruction);
            // RISCV::Registers::ABI rd = register_allocator->use_register_w(instr->target, instrs);
            // if (instr->source->value_type == Backend::LIR::OperandType::CONSTANT) {
            //     std::shared_ptr<Backend::LIR::Constant> constant = std::static_pointer_cast<Backend::LIR::Constant>(instr->source);
            //     if (constant->constant_type == Backend::VariableType::FLOAT) {
            //     } else {
            //         instrs.push_back(std::make_shared<RISCV::Instructions::LoadImmediate>(rd, constant->int32_value));
            //     }
            // } else {
            //     RISCV::Registers::ABI rs = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->source), instrs);
            //     instrs.push_back(std::make_shared<RISCV::Instructions::Add>(rd, RISCV::Registers::ABI::ZERO, rs));
            // }
            break;
        }
        case Backend::LIR::InstructionType::LOAD: {
            std::shared_ptr<Backend::LIR::LoadInt> instr = std::static_pointer_cast<Backend::LIR::LoadInt>(instruction);
            std::shared_ptr<Backend::Variable> addr = instr->var_in_mem;
            std::shared_ptr<Backend::Variable> dest = std::static_pointer_cast<Backend::Variable>(instr->var_in_reg);
            RISCV::Registers::ABI dest_reg = register_allocator->use_register_w(dest, instrs);
            if (addr->lifetime == Backend::VariableWide::GLOBAL) {
                instrs.push_back(std::make_shared<RISCV::Instructions::LoadAddress>(dest_reg, addr));
                instrs.push_back(std::make_shared<RISCV::Instructions::LoadWord>(dest_reg, dest_reg));
            } else {
                instrs.push_back(std::make_shared<RISCV::Instructions::LoadWordFromStack>(dest_reg, addr, stack));
            }
            break;
        }
        case Backend::LIR::InstructionType::STORE: {
            std::shared_ptr<Backend::LIR::StoreInt> instr = std::static_pointer_cast<Backend::LIR::StoreInt>(instruction);
            std::shared_ptr<Backend::Variable> dest = instr->var_in_mem;
            std::shared_ptr<Backend::Variable> src = std::static_pointer_cast<Backend::Variable>(instr->var_in_reg);
            RISCV::Registers::ABI src_reg = register_allocator->use_register_ro(src, instrs);
            instrs.push_back(std::make_shared<RISCV::Instructions::StoreWordToStack>(src_reg, dest, stack));
            break;
        }
        case Backend::LIR::InstructionType::CALL: {
            // std::shared_ptr<Backend::LIR::Call> instr = std::static_pointer_cast<Backend::LIR::Call>(instruction);
            // register_allocator->spill_caller_saved_registers(instrs);
            // std::vector<RISCV::Registers::ABI> param_regs = {
            //     RISCV::Registers::ABI::A0, RISCV::Registers::ABI::A1, RISCV::Registers::ABI::A2, RISCV::Registers::ABI::A3,
            //     RISCV::Registers::ABI::A4, RISCV::Registers::ABI::A5, RISCV::Registers::ABI::A6, RISCV::Registers::ABI::A7
            // };
            // if (instr->arguments) {
            //     auto& args = *instr->arguments;
            //     for (size_t i = 0; i < args.size() && i < param_regs.size(); ++i) {
            //         auto arg = args[i];
            //         if (arg->value_type == Backend::LIR::OperandType::CONSTANT) {
            //             // 常量参数直接加载到参数寄存器
            //             auto constant = std::static_pointer_cast<Backend::LIR::Constant>(arg);
            //             if (constant->is_int()) {
            //                 instrs.push_back(std::make_shared<RISCV::Instructions::LoadImmediate>(param_regs[i], constant->int32_value));
            //             }
            //             // 浮点常量暂时跳过
            //         } else {
            //             // 变量参数：先获取到临时寄存器，再移动到参数寄存器
            //             auto var = std::static_pointer_cast<Backend::Variable>(arg);
            //             RISCV::Registers::ABI src_reg = register_allocator->use_register_ro(var, instrs);
            //             if (src_reg != param_regs[i]) {
            //                 instrs.push_back(std::make_shared<RISCV::Instructions::Add>(param_regs[i], RISCV::Registers::ABI::ZERO, src_reg));
            //             }
            //         }
            //     }
            //     // 如果参数超过8个，多余的参数需要压栈（暂时不处理）
            //     if (args.size() > param_regs.size()) {
            //         log_debug("Function call has more than 8 parameters, stack parameters not implemented yet.");
            //     }
            // }
            // std::shared_ptr<RISCV::Function> callee_function = *std::find_if(module->functions.begin(), module->functions.end(), [&instr](const std::shared_ptr<RISCV::Function>& function) { return function->mir_function_ == instr->function; });
            // instrs.push_back(std::make_shared<RISCV::Instructions::Call>(callee_function));
            // if (instr->result) {
            //     RISCV::Registers::ABI dest_reg = register_allocator->use_register_w(instr->result, instrs);
            //     if (dest_reg != RISCV::Registers::ABI::A0) {
            //         instrs.push_back(std::make_shared<RISCV::Instructions::Add>(dest_reg, RISCV::Registers::ABI::ZERO, RISCV::Registers::ABI::A0));
            //     }
            // }
            // register_allocator->restore_caller_saved_registers(instrs);
            break;
        }
        case Backend::LIR::InstructionType::PUTF: {
            // std::shared_ptr<Backend::LIR::CallInstruction> instr = std::static_pointer_cast<Backend::LIR::CallInstruction>(instruction);
            break;
        }
        case Backend::LIR::InstructionType::JUMP: {
            std::shared_ptr<Backend::LIR::JumpInstruction> instr = std::static_pointer_cast<Backend::LIR::JumpInstruction>(instruction);
            std::string target_block_name = instr->target_block->name;
            std::shared_ptr<RISCV::Block> target_block = find_block(target_block_name);
            instrs.push_back(std::make_shared<RISCV::Instructions::Jump>(target_block));
            break;
        }
        // case Backend::LIR::InstructionType::BRANCH_ON_EQUAL: {
        //     std::shared_ptr<Backend::LIR::BranchInstruction> branch_instr = std::static_pointer_cast<Backend::LIR::BranchInstruction>(instruction);
        //     std::shared_ptr<RISCV::Block> target_block = find_block(branch_instr->target_block->name);
        //     RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(branch_instr->lhs, instrs);
        //     RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(branch_instr->rhs, instrs);
        //     instrs.push_back(std::make_shared<RISCV::Instructions::BranchOnEqual>(rs1, rs2, target_block));
        //     break;
        // }
        // case Backend::LIR::InstructionType::BRANCH_ON_NOT_EQUAL: {
        //     std::shared_ptr<Backend::LIR::BranchInstruction> branch_instr = std::static_pointer_cast<Backend::LIR::BranchInstruction>(instruction);
        //     std::shared_ptr<RISCV::Block> target_block = find_block(branch_instr->target_block->name);
        //     RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(branch_instr->lhs, instrs);
        //     RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(branch_instr->rhs, instrs);
        //     instrs.push_back(std::make_shared<RISCV::Instructions::BranchOnNotEqual>(rs1, rs2, target_block));
        //     break;
        // }
        // case Backend::LIR::InstructionType::BRANCH_ON_LESS_THAN: {
        //     std::shared_ptr<Backend::LIR::BranchInstruction> branch_instr = std::static_pointer_cast<Backend::LIR::BranchInstruction>(instruction);
        //     std::shared_ptr<RISCV::Block> target_block = find_block(branch_instr->target_block->name);
        //     RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(branch_instr->lhs, instrs);
        //     RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(branch_instr->rhs, instrs);
        //     instrs.push_back(std::make_shared<RISCV::Instructions::BranchOnLessThan>(rs1, rs2, target_block));
        //     break;
        // }
        // case Backend::LIR::InstructionType::BRANCH_ON_LESS_THAN_OR_EQUAL: {
        //     std::shared_ptr<Backend::LIR::BranchInstruction> branch_instr = std::static_pointer_cast<Backend::LIR::BranchInstruction>(instruction);
        //     std::shared_ptr<RISCV::Block> target_block = find_block(branch_instr->target_block->name);
        //     RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(branch_instr->lhs, instrs);
        //     RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(branch_instr->rhs, instrs);
        //     instrs.push_back(std::make_shared<RISCV::Instructions::BranchOnLessThanOrEqual>(rs1, rs2, target_block));
        //     break;
        // }
        // case Backend::LIR::InstructionType::BRANCH_ON_GREATER_THAN: {
        //     std::shared_ptr<Backend::LIR::BranchInstruction> branch_instr = std::static_pointer_cast<Backend::LIR::BranchInstruction>(instruction);
        //     std::shared_ptr<RISCV::Block> target_block = find_block(branch_instr->target_block->name);
        //     RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(branch_instr->lhs, instrs);
        //     RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(branch_instr->rhs, instrs);
        //     instrs.push_back(std::make_shared<RISCV::Instructions::BranchOnGreaterThan>(rs1, rs2, target_block));
        //     break;
        // }
        // case Backend::LIR::InstructionType::BRANCH_ON_GREATER_THAN_OR_EQUAL: {
        //     std::shared_ptr<Backend::LIR::BranchInstruction> branch_instr = std::static_pointer_cast<Backend::LIR::BranchInstruction>(instruction);
        //     std::shared_ptr<RISCV::Block> target_block = find_block(branch_instr->target_block->name);
        //     RISCV::Registers::ABI rs1 = register_allocator->use_register_ro(branch_instr->lhs, instrs);
        //     RISCV::Registers::ABI rs2 = register_allocator->use_register_ro(branch_instr->rhs, instrs);
        //     instrs.push_back(std::make_shared<RISCV::Instructions::BranchOnGreaterThanOrEqual>(rs1, rs2, target_block));
        //     break;
        // }
        // case Backend::LIR::InstructionType::RETURN: {
        //     std::shared_ptr<Backend::LIR::ReturnInstruction> instr = std::static_pointer_cast<Backend::LIR::ReturnInstruction>(instruction);
        //     if (instr->return_value) {
        //         switch (instr->return_value->operand_type) {
        //             case Backend::LIR::OperandType::VARIABLE: {
        //                 RISCV::Registers::ABI rs = register_allocator->use_register_ro(std::static_pointer_cast<Backend::Variable>(instr->return_value), instrs);
        //                 if (rs != RISCV::Registers::ABI::A0)
        //                     instrs.push_back(std::make_shared<RISCV::Instructions::Add>(RISCV::Registers::ABI::A0, RISCV::Registers::ABI::ZERO, rs));
        //                 break;
        //             }
        //             case Backend::LIR::OperandType::CONSTANT: {
        //                 instrs.push_back(std::make_shared<RISCV::Instructions::LoadImmediate>(RISCV::Registers::ABI::A0, std::static_pointer_cast<Backend::LIR::Constant>(instr->return_value)->int32_value));
        //                 break;
        //             }
        //         }
        //     }
        //     instrs.push_back(std::make_shared<RISCV::Instructions::FreeStack>(stack));
        //     instrs.push_back(std::make_shared<Instructions::Ret>());
        //     break;
        // }
        default: break;
    }
    return instrs;
}