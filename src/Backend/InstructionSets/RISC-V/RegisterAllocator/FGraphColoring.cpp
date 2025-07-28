#include "Backend/InstructionSets/RISC-V/RegisterAllocator/FGraphColoring.h"
#include "Backend/InstructionSets/RISC-V/Modules.h"

void RISCV::RegisterAllocator::FGraphColoring::allocate() {
    available_colors.insert(available_colors.end(), RISCV::Registers::Floats::registers.begin(), RISCV::Registers::Floats::registers.end());
    create_registers();
    build_interference_graph();
    const size_t K = available_colors.size();
    std::stack<std::string> simplify_stack;
    while (true) {
        simplify_phase(simplify_stack, K);
        if (coalesce_phase(K)) continue;
        else if (freeze_phase(K)) continue;
        else if (spill_phase(simplify_stack, K)) continue;
        else if (assign_colors<Backend::LIR::StoreFloat, Backend::LIR::LoadFloat>(simplify_stack)) break;
    }
    __allocate__();
    log_info("Allocated float registers for %s", lir_function->name.c_str());
}

void RISCV::RegisterAllocator::FGraphColoring::create_registers() {
    create_entry();
    std::shared_ptr<Backend::LIR::Block> block_entry = lir_function->blocks.front();
    // 2. add fa0-ft11
    for (const RISCV::Registers::ABI reg : RISCV::Registers::Floats::registers)
        lir_function->add_variable(std::make_shared<Backend::Variable>(RISCV::Registers::to_string(reg), Backend::VariableType::DOUBLE, Backend::VariableWide::LOCAL));
    // 3. move parameters
    for (size_t i = 0, j = 0; i < lir_function->parameters.size(); i++) {
        const std::shared_ptr<Backend::Variable> &arg = lir_function->parameters[i];
        if (Backend::Utils::is_float(arg->workload_type)) {
            if (j < 8) {
                block_entry->instructions.push_back(std::make_shared<Backend::LIR::Move>(lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::FA0 + j++)], arg));
            } else {
                // TODO
            }
        }
    }
    // at the very beginning of the function, copy callee-saved registers
    for (const RISCV::Registers::ABI reg : RISCV::Registers::Floats::callee_saved) {
        std::shared_ptr<Backend::Variable> var = std::make_shared<Backend::Variable>(RISCV::Registers::to_string(reg) + "_mem", Backend::VariableType::DOUBLE, Backend::VariableWide::LOCAL);
        lir_function->add_variable(var);
        block_entry->instructions.insert(block_entry->instructions.begin(), std::make_shared<Backend::LIR::Move>(lir_function->variables[RISCV::Registers::to_string(reg)], var));
    }
    for (const std::shared_ptr<Backend::LIR::Block> &block : lir_function->blocks) {
        for (size_t i = 0; i < block->instructions.size(); i++) {
            std::shared_ptr<Backend::LIR::Instruction> instruction = block->instructions[i];
            if (instruction->type == Backend::LIR::InstructionType::RETURN) {
                // move return value to fa0
                std::shared_ptr<Backend::LIR::Return> ret = std::static_pointer_cast<Backend::LIR::Return>(instruction);
                if (ret->return_value && Backend::Utils::is_float(ret->return_value->workload_type))
                    block->instructions.insert(block->instructions.begin() + i++, std::make_shared<Backend::LIR::Move>(ret->return_value, lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::FA0)])),
                    ret->return_value = lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::FA0)];
                for (const RISCV::Registers::ABI reg : RISCV::Registers::Floats::callee_saved)
                    block->instructions.insert(block->instructions.begin() + i++, std::make_shared<Backend::LIR::Move>(lir_function->variables[RISCV::Registers::to_string(reg) + "_mem"], lir_function->variables[RISCV::Registers::to_string(reg)]));
            } else if (instruction->type == Backend::LIR::InstructionType::CALL) {
                std::shared_ptr<Backend::LIR::Call> call = std::static_pointer_cast<Backend::LIR::Call>(block->instructions[i]);
                for (size_t j = 0, k = 0; j < call->arguments.size(); j++) {
                    const std::shared_ptr<Backend::Variable> &arg = call->arguments[j];
                    if (Backend::Utils::is_float(arg->workload_type)) {
                        if (k < 8) {
                            // move arguments to fa0-fa7
                            const std::shared_ptr<Backend::Variable> &phyReg = lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::FA0 + k++)];
                            block->instructions.insert(block->instructions.begin() + i++, std::make_shared<Backend::LIR::Move>(call->arguments[j], phyReg));
                            call->arguments[j] = phyReg;
                        } else {
                            // TODO
                        }
                    }
                }
                // move result of the call to fa0
                if (call->result && Backend::Utils::is_float(call->result->workload_type))
                    block->instructions.insert(block->instructions.begin() + i + 1, std::make_shared<Backend::LIR::Move>(lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::FA0)], call->result)),
                    call->result = lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::FA0)];
            }
        }
    }
}

void RISCV::RegisterAllocator::FGraphColoring::build_interference_graph() {
    create_interference_nodes(RISCV::Registers::Floats::registers);
    lir_function->analyze_live_variables<Backend::Utils::is_float>();
    GraphColoring::build_interference_graph<>(RISCV::Registers::Floats::caller_saved);
}