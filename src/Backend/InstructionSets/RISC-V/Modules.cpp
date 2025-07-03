#include "Backend/InstructionSets/RISC-V/Modules.h"
#include "Backend/MIR/Instructions.h"
#include "Utils/Log.h"

RISCV::Module::Module(const std::shared_ptr<Backend::MIR::Module>& mir_module, const RegisterAllocator::AllocationType& allocation_type) {
    data_section = std::move(mir_module->global_data);
    for (const std::shared_ptr<Backend::MIR::Function>& mir_function : mir_module->functions) {
        if (mir_function->function_type == Backend::MIR::FunctionType::PRIVILEGED) {
            continue;
        }
        functions.push_back(std::make_shared<RISCV::Function>(mir_function, allocation_type));
    }
    for (std::shared_ptr<RISCV::Function>& function : functions) {
        function->to_assembly();
    }
}

RISCV::Function::Function(const std::shared_ptr<Backend::MIR::Function> &mir_function, const RegisterAllocator::AllocationType& allocation_type) : name(mir_function->name), mir_function_(mir_function) {
    stack = std::make_shared<RISCV::Stack>();
    register_allocator = RISCV::RegisterAllocator::create(allocation_type, mir_function, stack);
    register_allocator->allocate();
}

std::string RISCV::Block::to_string() const {
    std::ostringstream oss;
    oss << " ." << function.lock()->name << "_" << name << ":\n";
    for (const std::shared_ptr<Instructions::Instruction>& instr : instructions) {
        oss << "  " << instr->to_string() << "\n";
    }
    return oss.str();
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
    block_entry->instructions.push_back(std::make_shared<Instructions::StoreSP>(stack));
}

void RISCV::Function::translate_blocks() {
    for (const std::shared_ptr<Backend::MIR::Block> &block : mir_function_->blocks) {
        blocks.push_back(std::make_shared<RISCV::Block>(block->name, shared_from_this()));
    }
    for (const std::shared_ptr<Backend::MIR::Block> &mir_block : mir_function_->blocks) {
        std::shared_ptr<RISCV::Block> block = find_block(mir_block->name);
        for (const std::shared_ptr<Backend::MIR::Instruction> &instr: mir_block->instructions) {
            std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> instructions = translate_instruction(instr);
            block->instructions.insert(block->instructions.end(), instructions.begin(), instructions.end());
        }
    }
}

std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> RISCV::Function::translate_instruction(const std::shared_ptr<Backend::MIR::Instruction>& instruction) {
    std::vector<std::shared_ptr<RISCV::Instructions::Instruction>> instrs;
    switch (instruction->type) {
        case Backend::MIR::InstructionType::ADD: {
            std::shared_ptr<Backend::MIR::ArithmeticInstruction> instr = std::static_pointer_cast<Backend::MIR::ArithmeticInstruction>(instruction);
            std::shared_ptr<Backend::MIR::Value> lhs = instr->lhs;
            std::shared_ptr<Backend::MIR::Value> rhs = instr->rhs;
            std::shared_ptr<Backend::MIR::Variable> result = instr->result;
            if (lhs->value_type == Backend::MIR::OperandType::CONSTANT && rhs->value_type == Backend::MIR::OperandType::CONSTANT) {
                log_error("Cannot add two constants in RISC-V backend");
            }
            if (lhs->value_type == Backend::MIR::OperandType::CONSTANT) {
                RISCV::Registers::ABI rs = register_allocator->get_register(std::static_pointer_cast<Backend::MIR::Variable>(rhs));
                RISCV::Registers::ABI rd = register_allocator->get_register(result);
                instrs.push_back(std::make_shared<RISCV::Instructions::AddImmediate>(rd, rs, std::static_pointer_cast<Backend::MIR::Constant>(lhs)->int32_value));
            } else if (rhs->value_type == Backend::MIR::OperandType::CONSTANT) {
                RISCV::Registers::ABI rs = register_allocator->get_register(std::static_pointer_cast<Backend::MIR::Variable>(lhs));
                RISCV::Registers::ABI rd = register_allocator->get_register(result);
                instrs.push_back(std::make_shared<RISCV::Instructions::AddImmediate>(rd, rs, std::static_pointer_cast<Backend::MIR::Constant>(rhs)->int32_value));
            } else {
                RISCV::Registers::ABI rs1 = register_allocator->get_register(std::static_pointer_cast<Backend::MIR::Variable>(lhs));
                RISCV::Registers::ABI rs2 = register_allocator->get_register(std::static_pointer_cast<Backend::MIR::Variable>(rhs));
                RISCV::Registers::ABI rd = register_allocator->get_register(result);
                instrs.push_back(std::make_shared<RISCV::Instructions::Add>(rd, rs1, rs2));
            }
            break;
        }
        case Backend::MIR::InstructionType::SUB: {
            std::shared_ptr<Backend::MIR::ArithmeticInstruction> instr = std::static_pointer_cast<Backend::MIR::ArithmeticInstruction>(instruction);
            std::shared_ptr<Backend::MIR::Value> lhs = instr->lhs;
            std::shared_ptr<Backend::MIR::Value> rhs = instr->rhs;
            std::shared_ptr<Backend::MIR::Variable> result = instr->result;
            if (lhs->value_type == Backend::MIR::OperandType::CONSTANT && rhs->value_type == Backend::MIR::OperandType::CONSTANT) {
                log_error("Cannot sub two constants in RISC-V backend");
            }
            if (lhs->value_type == Backend::MIR::OperandType::CONSTANT) {
                RISCV::Registers::ABI rs = register_allocator->get_register(std::static_pointer_cast<Backend::MIR::Variable>(rhs));
                RISCV::Registers::ABI rd = register_allocator->get_register(result);
                instrs.push_back(std::make_shared<RISCV::Instructions::AddImmediate>(rd, rs, -std::static_pointer_cast<Backend::MIR::Constant>(lhs)->int32_value));
            } else if (rhs->value_type == Backend::MIR::OperandType::CONSTANT) {
                RISCV::Registers::ABI rs = register_allocator->get_register(std::static_pointer_cast<Backend::MIR::Variable>(lhs));
                RISCV::Registers::ABI rd = register_allocator->get_register(result);
                instrs.push_back(std::make_shared<RISCV::Instructions::AddImmediate>(rd, rs, -std::static_pointer_cast<Backend::MIR::Constant>(rhs)->int32_value));
            } else {
                RISCV::Registers::ABI rs1 = register_allocator->get_register(std::static_pointer_cast<Backend::MIR::Variable>(lhs));
                RISCV::Registers::ABI rs2 = register_allocator->get_register(std::static_pointer_cast<Backend::MIR::Variable>(rhs));
                RISCV::Registers::ABI rd = register_allocator->get_register(result);
                instrs.push_back(std::make_shared<RISCV::Instructions::Add>(rd, rs1, rs2));
            }
            break;
        }
        case Backend::MIR::InstructionType::MUL: {
            break;
        }
        case Backend::MIR::InstructionType::DIV: {
            break;
        }
        case Backend::MIR::InstructionType::MOD: {
            break;
        }
        case Backend::MIR::InstructionType::LOAD: {
            std::shared_ptr<Backend::MIR::MemoryInstruction> instr = std::static_pointer_cast<Backend::MIR::MemoryInstruction>(instruction);
            std::shared_ptr<Backend::MIR::Variable> addr = instr->var_in_mem;
            std::shared_ptr<Backend::MIR::Variable> dest = std::static_pointer_cast<Backend::MIR::Variable>(instr->var_in_reg);
            RISCV::Registers::ABI dest_reg = register_allocator->get_register(dest);
            if (addr->position == Backend::MIR::VariablePosition::GLOBAL) {
                instrs.push_back(std::make_shared<RISCV::Instructions::LoadAddress>(dest_reg, addr));
                instrs.push_back(std::make_shared<RISCV::Instructions::LoadWord>(dest_reg, dest_reg));
            } else {
                instrs.push_back(std::make_shared<RISCV::Instructions::LoadWordFromStack>(dest_reg, addr, stack));
            }
            break;
        }
        case Backend::MIR::InstructionType::STORE: {
            // auto mem_instr = std::static_pointer_cast<Backend::MIR::MemoryInstruction>(instruction);
            // auto addr = mem_instr->memory_operand;
            // auto src = mem_instr->register_operand;
            // // 确定源寄存器和地址寄存器
            // std::shared_ptr<Backend::MIR::Variable> rs1, rs2;
            // if (src->value_type == Backend::MIR::OperandType::CONSTANT) {
            //     // 如果源是立即数，加载到临时寄存器
            //     auto temp_reg = std::make_shared<Backend::MIR::Variable>("5", Backend::VariableType::INT64); // t0 = x5
            //     auto imm = std::static_pointer_cast<Backend::MIR::Constant>(src);
            //     instructions.push_back(std::make_shared<Instructions::LoadImmediate>(temp_reg, imm));
            //     rs2 = temp_reg;
            // } else {
            //     // 源是变量
            //     auto src_name = std::static_pointer_cast<Backend::MIR::Variable>(src)->name;
            //     rs2 = std::make_shared<Backend::MIR::Variable>(
            //         std::to_string(static_cast<int>(register_allocator->get_register(src_name))), src->type);
            // }
            
            // // 地址寄存器
            // rs1 = std::make_shared<Backend::MIR::Variable>(
            //     std::to_string(static_cast<int>(register_allocator->get_register(addr->name))), addr->type);
            
            // // 存储指令
            // auto offset = std::make_shared<Backend::MIR::Constant>(0);
            // if (src->type == Backend::VariableType::INT64) {
            //     instructions.push_back(std::make_shared<Instructions::StoreDoubleword>(rs1, rs2, offset));
            // } else {
            //     instructions.push_back(std::make_shared<Instructions::StoreWord>(rs1, rs2, offset));
            // }
            break;
        }
        case Backend::MIR::InstructionType::CALL: {
            // auto call_instr = std::static_pointer_cast<Backend::MIR::CallInstruction>(instruction);
            // // 复制参数到参数寄存器
            // auto params = call_instr->params;
            // for (size_t i = 0; i < params->size(); ++i) {
            //     auto param = (*params)[i];
            //     auto a_reg = std::make_shared<Backend::MIR::Variable>(std::to_string(static_cast<int>(RISCV::Registers::ABI::A0) + i), Backend::VariableType::INT64);
                
            //     if (param->value_type == Backend::MIR::OperandType::CONSTANT) {
            //         // 如果参数是立即数
            //         auto imm = std::static_pointer_cast<Backend::MIR::Constant>(param);
            //         instructions.push_back(std::make_shared<Instructions::LoadImmediate>(a_reg, imm));
            //     } else {
            //         // 如果参数是变量
            //         auto var = std::static_pointer_cast<Backend::MIR::Variable>(param);
            //         auto rs = std::make_shared<Backend::MIR::Variable>(
            //             std::to_string(static_cast<int>(register_allocator->get_register(var->name))), var->type);
                    
            //         // 移动到参数寄存器
            //         // TODO: 实现寄存器间移动
            //     }
            // }
            
            // // 调用函数
            // std::string function_name = call_instr->function->name;
            // instructions.push_back(std::make_shared<Instructions::Call>(function_name));
            
            // // 处理返回值
            // if (call_instr->result) {
            //     // TODO: 保存a0中的返回值到目标寄存器
            // }
            break;
        }
        case Backend::MIR::InstructionType::JUMP: {
            // auto branch_instr = std::static_pointer_cast<Backend::MIR::BranchInstruction>(instruction);
            // auto target_block = branch_instr->target_block;
            // std::string label = name + "_" + target_block->name;
            // instructions.push_back(std::make_shared<Instructions::Jump>(label));
            // break;
        }
        case Backend::MIR::InstructionType::BRANCH_ON_EQUAL:
        case Backend::MIR::InstructionType::BRANCH_ON_NOT_EQUAL:
        case Backend::MIR::InstructionType::BRANCH_ON_LESS_THAN:
        case Backend::MIR::InstructionType::BRANCH_ON_LESS_THAN_OR_EQUAL:
        case Backend::MIR::InstructionType::BRANCH_ON_GREATER_THAN:
        case Backend::MIR::InstructionType::BRANCH_ON_GREATER_THAN_OR_EQUAL: {
            // auto branch_instr = std::static_pointer_cast<Backend::MIR::BranchInstruction>(instruction);
            // // 获取条件变量
            // auto cond_var = branch_instr->cond;
            // auto target_block = branch_instr->target_block;
            // std::string label = name + "_" + target_block->name;
            
            // // 处理条件分支
            // if (auto compare_var = std::dynamic_pointer_cast<Backend::MIR::CompareVariable>(cond_var)) {
            //     auto lhs = compare_var->lhs;
            //     auto rhs = compare_var->rhs;
                
            //     // 确定源寄存器
            //     std::shared_ptr<Backend::MIR::Variable> rs1, rs2;
                
            //     if (lhs->value_type == Backend::MIR::OperandType::CONSTANT) {
            //         // 如果左操作数是立即数
            //         auto temp_reg = std::make_shared<Backend::MIR::Variable>("5", Backend::VariableType::INT64); // t0 = x5
            //         auto imm = std::static_pointer_cast<Backend::MIR::Constant>(lhs);
            //         instructions.push_back(std::make_shared<Instructions::LoadImmediate>(temp_reg, imm));
            //         rs1 = temp_reg;
            //     } else {
            //         // 左操作数是变量
            //         auto lhs_name = std::static_pointer_cast<Backend::MIR::Variable>(lhs)->name;
            //         rs1 = std::make_shared<Backend::MIR::Variable>(
            //             std::to_string(static_cast<int>(register_allocator->get_register(lhs_name))), lhs->type);
            //     }
                
            //     if (rhs->value_type == Backend::MIR::OperandType::CONSTANT) {
            //         // 如果右操作数是立即数
            //         auto temp_reg = std::make_shared<Backend::MIR::Variable>("6", Backend::VariableType::INT64); // t1 = x6
            //         auto imm = std::static_pointer_cast<Backend::MIR::Constant>(rhs);
            //         instructions.push_back(std::make_shared<Instructions::LoadImmediate>(temp_reg, imm));
            //         rs2 = temp_reg;
            //     } else {
            //         // 右操作数是变量
            //         auto rhs_name = std::static_pointer_cast<Backend::MIR::Variable>(rhs)->name;
            //         rs2 = std::make_shared<Backend::MIR::Variable>(
            //             std::to_string(static_cast<int>(register_allocator->get_register(rhs_name))), rhs->type);
            //     }
                
            //     // 根据比较类型生成分支指令
            //     switch (compare_var->compare_type) {
            //         case InstructionType::BRANCH_ON_EQUAL:
            //             instructions.push_back(std::make_shared<Instructions::BranchOnEqual>(rs1, rs2, label));
            //             break;
            //         case InstructionType::BRANCH_ON_NOT_EQUAL:
            //             instructions.push_back(std::make_shared<Instructions::BranchOnNotEqual>(rs1, rs2, label));
            //             break;
            //         case InstructionType::BRANCH_ON_LESS_THAN:
            //             instructions.push_back(std::make_shared<Instructions::BranchOnLessThan>(rs1, rs2, label));
            //             break;
            //         case InstructionType::BRANCH_ON_LESS_THAN_OR_EQUAL:
            //             instructions.push_back(std::make_shared<Instructions::BranchOnLessThanOrEqual>(rs1, rs2, label));
            //             break;
            //         case InstructionType::BRANCH_ON_GREATER_THAN:
            //             instructions.push_back(std::make_shared<Instructions::BranchOnGreaterThan>(rs1, rs2, label));
            //             break;
            //         case InstructionType::BRANCH_ON_GREATER_THAN_OR_EQUAL:
            //             instructions.push_back(std::make_shared<Instructions::BranchOnGreaterThanOrEqual>(rs1, rs2, label));
            //             break;
            //         default:
            //             log_error("Unsupported branch condition type");
            //             break;
            //     }
            // }
            break;
        }
        case Backend::MIR::InstructionType::RETURN: {
            std::shared_ptr<Backend::MIR::ReturnInstruction> instr = std::static_pointer_cast<Backend::MIR::ReturnInstruction>(instruction);
            if (instr->return_value) {
                switch (instr->return_value->value_type) {
                    case Backend::MIR::OperandType::VARIABLE: {
                        RISCV::Registers::ABI rs = register_allocator->get_register(std::static_pointer_cast<Backend::MIR::Variable>(instr->return_value));
                        if (rs != RISCV::Registers::ABI::A0)
                            instrs.push_back(std::make_shared<RISCV::Instructions::Add>(RISCV::Registers::ABI::A0, RISCV::Registers::ABI::ZERO, rs));
                        break;
                    }
                    case Backend::MIR::OperandType::CONSTANT: {
                        instrs.push_back(std::make_shared<RISCV::Instructions::LoadImmediate>(RISCV::Registers::ABI::A0, std::static_pointer_cast<Backend::MIR::Constant>(instr->return_value)->int32_value));
                        break;
                    }
                }
            }
            // int total_size = register_allocator->stack_size + 16;
            // auto offset_ra = std::make_shared<Backend::MIR::Constant>(total_size - 8);
            // auto offset_sp = std::make_shared<Backend::MIR::Constant>(total_size - 16);
            // instrs.push_back(std::make_shared<Instructions::LoadDoubleword>(RISCV::Registers::ABI::RA, RISCV::Registers::ABI::SP, offset_ra));
            // instrs.push_back(std::make_shared<Instructions::LoadDoubleword>(RISCV::Registers::ABI::SP, RISCV::Registers::ABI::SP, offset_sp));
            // auto size_imm = std::make_shared<Backend::MIR::Constant>(total_size);
            // instrs.push_back(std::make_shared<Instructions::AddImmediate>(RISCV::Registers::ABI::SP, RISCV::Registers::ABI::SP, size_imm));
            instrs.push_back(std::make_shared<Instructions::Ret>());
            break;
        }
        default: break;
    }
    return instrs;
}

std::string RISCV::Function::to_string() const {
    std::ostringstream oss;
    oss << ".function_" << name << ":\n";
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