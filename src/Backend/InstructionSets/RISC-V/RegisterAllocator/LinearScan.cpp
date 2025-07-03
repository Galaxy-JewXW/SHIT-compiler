#include "Backend/InstructionSets/RISC-V/RegisterAllocator/LinearScan.h"
#include "Backend/InstructionSets/RISC-V/Modules.h"

void RISCV::RegisterAllocator::LinearScan::allocate() {
    std::vector<RISCV::RegisterAllocator::LinearScan::LiveInterval> live_intervals_global = analyze_live_intervals(true);
    allocate(live_intervals_global, available_global_regs);
    std::vector<RISCV::RegisterAllocator::LinearScan::LiveInterval> live_intervals_local = analyze_live_intervals(false);
    allocate(live_intervals_local, available_temp_regs);
    log_debug("Register allocation completed for function %s.", mir_function->name.c_str());
    for (std::pair<std::string, std::shared_ptr<Backend::MIR::Variable>> var: mir_function->variables) {
        if (var_to_reg.find(var.first) != var_to_reg.end()) {
            log_debug("Variable %s allocated to register %s.", var.first.c_str(), RISCV::Registers::reg2string(var_to_reg[var.first]).c_str());
        } else if (stack->stack_index.find(var.first) != stack->stack_index.end()) {
            log_debug("Variable %s allocated to stack.", var.first.c_str());
        } else {
            log_debug("Variable %s not allocated.", var.first.c_str());
        }
    }
}

void RISCV::RegisterAllocator::LinearScan::allocate(std::vector<RISCV::RegisterAllocator::LinearScan::LiveInterval> &live_intervals, std::vector<RISCV::Registers::ABI> &available_regs) {
    std::vector<std::pair<int, RISCV::Registers::ABI>> active;
    std::vector<RISCV::Registers::ABI> free_regs = available_regs;
    for (const LiveInterval &interval : live_intervals) {
        active.erase(std::remove_if(active.begin(), active.end(), [&](const std::pair<int, RISCV::Registers::ABI>& p) {
            return p.first < interval.start;
        }), active.end());
        if (!free_regs.empty()) {
            RISCV::Registers::ABI reg = free_regs.back();
            free_regs.pop_back();
            var_to_reg[interval.var_name] = reg;
            active.push_back({interval.end, reg});
        } else {
            // 在活跃区间中找到结束最晚的区间用于溢出
            auto spill_candidate = std::max_element(active.begin(), active.end(),
                [](const std::pair<int, RISCV::Registers::ABI>& a, 
                    const std::pair<int, RISCV::Registers::ABI>& b) {
                    return a.first < b.first;
                }
            );
            // 如果找到了溢出候选，且该候选比当前区间结束得更晚
            if (spill_candidate != active.end() && spill_candidate->first > interval.end) {
                std::string spill_var;
                for (const auto& [name, reg] : var_to_reg) {
                    if (reg == spill_candidate->second) {
                        spill_var = name;
                        break;
                    }
                }
                var_to_reg.erase(spill_var);
                stack->add_variable(mir_function->variables[spill_var]);
                var_to_reg[interval.var_name] = spill_candidate->second;
                // 从活跃列表中移除溢出的区间
                active.erase(spill_candidate);
                // 添加新区间到活跃列表
                active.push_back({interval.end, spill_candidate->second});
            } else {
                stack->add_variable(mir_function->variables[interval.var_name]);
            }
        }
    }
}

std::vector<RISCV::RegisterAllocator::LinearScan::LiveInterval> RISCV::RegisterAllocator::LinearScan::analyze_live_intervals(bool global) {
    int instr_index = 0;
    std::vector<LiveInterval> live_intervals;
    std::unordered_map<std::string, int> var_start;
    std::unordered_map<std::string, int> var_end;
    for (const std::shared_ptr<Backend::MIR::Block> &block : this->mir_function->blocks) {
        for (const std::shared_ptr<Backend::MIR::Instruction> &instr : block->instructions) {
            std::shared_ptr<Backend::MIR::Variable> defined_var = instr->get_defined_variable();
            if (is_global(global, defined_var)) {
                var_start[defined_var->name] = instr_index;
                var_end[defined_var->name] = instr_index;
            }
            std::shared_ptr<std::vector<std::shared_ptr<Backend::MIR::Variable>>> used_vars = instr->get_used_variables();
            for (const std::shared_ptr<Backend::MIR::Variable> &used_var : *used_vars) {
                if (is_global(global, used_var)) {
                    if (var_start.find(used_var->name) == var_start.end()) {
                        var_start[used_var->name] = instr_index;
                    }
                    var_end[used_var->name] = instr_index;
                }
            }
            instr_index++;
        }
    }
    for (const auto& [var_name, start] : var_start) {
        LiveInterval interval;
        interval.var_name = var_name;
        interval.start = start;
        interval.end = (var_end.find(var_name) != var_end.end()) ? var_end[var_name] : start;
        live_intervals.push_back(interval);
    }
    std::sort(live_intervals.begin(), live_intervals.end());
    return live_intervals;
}

RISCV::Registers::ABI RISCV::RegisterAllocator::LinearScan::use_register(const std::shared_ptr<Backend::MIR::Variable>& variable, std::vector<std::shared_ptr<RISCV::Instructions::Instruction>>& instrs) {
    RISCV::Registers::ABI reg = get_register(variable);
    // if (reg == RISCV::Registers::ABI::ZERO) {
    //         reg = RISCV::Registers::ABI::A0;
    //         auto var_name = std::static_pointer_cast<Backend::MIR::Variable>(variable)->name;
    //         auto offset = register_allocator->get_stack_offset(var_name);
    //         instrs.push_back(std::make_shared<RISCV::Instructions::LoadDoubleword>(reg, RISCV::Registers::ABI::SP, std::make_shared<Backend::MIR::Constant>(offset)));
    // }
    return reg;
}