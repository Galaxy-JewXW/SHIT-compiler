#include "Backend/InstructionSets/RISC-V/RegisterAllocator/GraphColoring.h"
#include "Backend/InstructionSets/RISC-V/RegisterAllocator/FGraphColoring.h"
#include "Backend/InstructionSets/RISC-V/Modules.h"

void RISCV::RegisterAllocator::GraphColoring::allocate() {
    float_allocator = std::make_shared<RISCV::RegisterAllocator::FGraphColoring>(lir_function, stack);
    float_allocator->allocate();
    var_to_reg.insert(float_allocator->var_to_reg.begin(), float_allocator->var_to_reg.end());
    available_colors.insert(available_colors.end(), RISCV::Registers::Integers::registers.begin(), RISCV::Registers::Integers::registers.end());
    create_registers();
    build_interference_graph();
    const size_t K = available_colors.size();
    std::stack<std::string> simplify_stack;
    while (true) {
        simplify_phase(simplify_stack, K);
        if (coalesce_phase(K)) continue;
        else if (freeze_phase(K)) continue;
        else if (spill_phase(simplify_stack, K)) continue;
        else if (assign_colors<Backend::LIR::StoreInt, Backend::LIR::LoadInt>(simplify_stack)) break;
    }
    __allocate__();
    log_info("Allocated integer registers for %s", lir_function->name.c_str());
    stack->stack_size = stack->align(16);
    std::cout << to_string() << "\n";
}

void RISCV::RegisterAllocator::GraphColoring::__allocate__() {
    for (const std::pair<std::string, std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode>> pair : interference_graph)
        if (pair.second->is_colored && pair.second->color != RISCV::Registers::ABI::ZERO) {
            var_to_reg[pair.first] = pair.second->color;
            for (const std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> &coalesced: pair.second->coalesced)
                var_to_reg[coalesced->variable->name] = pair.second->color;
        }
}

/*
 * Create virtual registers for all integer registers.
 * First, we create a block entry which will not be included in any loops.
 * Second, we create variables for reigsters.
 * Third, we insert `load` & `store` instructions for function calls & parameters.
 * Last, we save all caller-saved registers.
 */
void RISCV::RegisterAllocator::GraphColoring::create_registers() {
    RISCV::ReWrite::create_entry_block(lir_function);
    std::shared_ptr<Backend::LIR::Block> block_entry = lir_function->blocks.front();
    for (const RISCV::Registers::ABI reg : RISCV::Registers::Integers::registers)
        lir_function->add_variable(std::make_shared<Backend::Variable>(RISCV::Registers::to_string(reg), Backend::VariableType::INT64, Backend::VariableWide::LOCAL));
    RISCV::ReWrite::rewrite_parameters(lir_function, stack);
    // at the very beginning of the function, copy callee-saved registers
    for (const RISCV::Registers::ABI reg : RISCV::Registers::Integers::callee_saved) {
        std::shared_ptr<Backend::Variable> var = std::make_shared<Backend::Variable>(RISCV::Registers::to_string(reg) + "_mem", Backend::VariableType::INT64, Backend::VariableWide::LOCAL);
        lir_function->add_variable(var);
        block_entry->instructions.insert(block_entry->instructions.begin(), std::make_shared<Backend::LIR::Move>(lir_function->variables[RISCV::Registers::to_string(reg)], var));
    }
    for (const std::shared_ptr<Backend::LIR::Block> &block : lir_function->blocks) {
        for (size_t i = 0; i < block->instructions.size(); i++) {
            std::shared_ptr<Backend::LIR::Instruction> instruction = block->instructions[i];
            if (instruction->type == Backend::LIR::InstructionType::RETURN) {
                // move return value to a0
                std::shared_ptr<Backend::LIR::Return> ret = std::static_pointer_cast<Backend::LIR::Return>(instruction);
                if (ret->return_value && Backend::Utils::is_int(ret->return_value->workload_type))
                    block->instructions.insert(block->instructions.begin() + i++, std::make_shared<Backend::LIR::Move>(ret->return_value, lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::A0)])),
                    ret->return_value = lir_function->variables[RISCV::Registers::to_string(RISCV::Registers::ABI::A0)];
                for (const RISCV::Registers::ABI reg : RISCV::Registers::Integers::callee_saved)
                    block->instructions.insert(block->instructions.begin() + i++, std::make_shared<Backend::LIR::Move>(lir_function->variables[RISCV::Registers::to_string(reg) + "_mem"], lir_function->variables[RISCV::Registers::to_string(reg)]));
            }
        }
    }
}

void RISCV::RegisterAllocator::GraphColoring::create_interference_nodes(const std::vector<RISCV::Registers::ABI> &registers) {
    interference_graph.clear();
    for (const auto& [var_name, var] : lir_function->variables)
        if (var->lifetime == Backend::VariableWide::LOCAL && is_consistent(var->workload_type))
            interference_graph[var_name] = std::make_shared<InterferenceNode>(var);
    for (const RISCV::Registers::ABI reg : registers) {
        interference_graph[RISCV::Registers::to_string(reg)]->is_colored = true;
        interference_graph[RISCV::Registers::to_string(reg)]->color = reg;
    }
    for (const RISCV::Registers::ABI reg1 : registers)
        for (const RISCV::Registers::ABI reg2 : registers)
            if (reg1 != reg2)
                interference_graph[RISCV::Registers::to_string(reg1)]->non_move_related_neighbors.insert(interference_graph[RISCV::Registers::to_string(reg2)]);
}

void RISCV::RegisterAllocator::GraphColoring::build_interference_graph() {
    RISCV::ReWrite::rewrite_large_offset(lir_function, stack);
    create_interference_nodes(RISCV::Registers::Integers::registers);
    lir_function->analyze_live_variables<Backend::Utils::is_int>();
    build_interference_graph<>(RISCV::Registers::Integers::caller_saved);
}

template <size_t N>
void RISCV::RegisterAllocator::GraphColoring::build_interference_graph(const std::array<RISCV::Registers::ABI, N> &caller_saved) {
    for (const std::shared_ptr<Backend::LIR::Block> &block : lir_function->blocks) {
        std::unordered_set<std::string> live = block->live_out;
        // Reverse traversal!!!
        for (std::vector<std::shared_ptr<Backend::LIR::Instruction>>::reverse_iterator instr_iter = block->instructions.rbegin(); instr_iter != block->instructions.rend(); ++instr_iter) {
            const std::shared_ptr<Backend::LIR::Instruction> &instr = *instr_iter;
            std::shared_ptr<Backend::Variable> def_var = instr->get_defined_variable(is_consistent);
            std::vector<std::shared_ptr<Backend::Variable>> used_vars = instr->get_used_variables(is_consistent);
            bool is_move_instruction = (instr->type == Backend::LIR::InstructionType::MOVE || instr->type == Backend::LIR::InstructionType::FMOVE);
            if (def_var) {
                live.erase(def_var->name);
                for (const std::string &live_var : live) {
                    interference_graph[def_var->name]->non_move_related_neighbors.insert(interference_graph[live_var]);
                    interference_graph[live_var]->non_move_related_neighbors.insert(interference_graph[def_var->name]);
                }
                if (is_move_instruction) {
                    for (const std::shared_ptr<Backend::Variable> &used_var : used_vars) {
                        interference_graph[def_var->name]->move_related_neighbors.insert(interference_graph[used_var->name]);
                        interference_graph[used_var->name]->move_related_neighbors.insert(interference_graph[def_var->name]);
                    }
                }
            }
            // if the instruction is a function call, all live variables conflict with caller-saved registers
            if (instr->type == Backend::LIR::InstructionType::CALL)
                for (const RISCV::Registers::ABI reg : caller_saved)
                    for (const std::string &live_var : live) {
                        interference_graph[RISCV::Registers::to_string(reg)]->non_move_related_neighbors.insert(interference_graph[live_var]);
                        interference_graph[live_var]->non_move_related_neighbors.insert(interference_graph[RISCV::Registers::to_string(reg)]);
                    }
            for (const std::shared_ptr<Backend::Variable> &used_var : used_vars)
                live.insert(used_var->name);
        }
    }
    for (const auto& [var_name, node] : interference_graph)
        for (const std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> &neighbor : node->non_move_related_neighbors)
            node->move_related_neighbors.erase(neighbor);
    calculate_spill_costs();
    // print_interference_graph();
}

template void RISCV::RegisterAllocator::GraphColoring::build_interference_graph<RISCV::Registers::Floats::caller_saved.size()>(const std::array<RISCV::Registers::ABI, RISCV::Registers::Floats::caller_saved.size()>&);

void RISCV::RegisterAllocator::GraphColoring::print_interference_graph() {
    std::ostringstream oss;
    oss << "\n";
    for (const auto& [var_name, node] : interference_graph) {
        oss << "Node: " << var_name << "\n";
        oss << "  Move-related neighbors:";
        for (const std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> &neighbor : node->move_related_neighbors)
            oss << " " << neighbor->variable->name;
        oss << "\n";
        oss << "  Non-move-related neighbors:";
        for (const std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> &neighbor : node->non_move_related_neighbors)
            oss << " " << neighbor->variable->name;
        oss << "\n";
    }
    std::cout << oss.str();
}

void RISCV::RegisterAllocator::GraphColoring::calculate_spill_costs() {
    spill_costs.clear();
    for (const auto& [var_name, var] : lir_function->variables)
        if (var->lifetime == Backend::VariableWide::LOCAL && is_consistent(var->workload_type))
            spill_costs[var_name] = SpillCost{};
    size_t instr_idx = 0;
    std::unordered_map<std::string, size_t> first_use;
    std::unordered_map<std::string, size_t> last_use;
    for (const std::shared_ptr<Backend::LIR::Block> &block : lir_function->blocks) {
        for (const std::shared_ptr<Backend::LIR::Instruction> &instr : block->instructions) {
            std::shared_ptr<Backend::Variable> def_var = instr->get_defined_variable(is_consistent);
            if (def_var && spill_costs.find(def_var->name) != spill_costs.end()) {
                spill_costs[def_var->name].def_count++;
                if (first_use.find(def_var->name) == first_use.end())
                    first_use[def_var->name] = instr_idx;
                last_use[def_var->name] = instr_idx;
            }
            std::vector<std::shared_ptr<Backend::Variable>> used_vars = instr->get_used_variables(is_consistent);
            for (const std::shared_ptr<Backend::Variable> &used_var : used_vars)
                if (spill_costs.find(used_var->name) != spill_costs.end()) {
                    spill_costs[used_var->name].use_count++;
                    if (first_use.find(used_var->name) == first_use.end())
                        first_use[used_var->name] = instr_idx;
                    last_use[used_var->name] = instr_idx;
                }
            instr_idx++;
        }
    }
    for (auto& [var_name, cost] : spill_costs) {
        if (first_use.find(var_name) != first_use.end() && last_use.find(var_name) != last_use.end())
            cost.live_range = last_use[var_name] - first_use[var_name] + 1;
        cost.loop_depth = 1; // TODO: Implement loop depth analysis
        cost.cost = calculate_spill_cost(cost);
    }
}

double RISCV::RegisterAllocator::GraphColoring::calculate_spill_cost(const RISCV::RegisterAllocator::GraphColoring::SpillCost &cost_info) {
    // cost = (use_count + def_count * 2) * pow(10, loop_depth) / live_range
    double access_count = cost_info.use_count + cost_info.def_count * 2.0;
    double loop_factor = std::pow(10.0, cost_info.loop_depth);
    double range_factor = std::max(1.0, static_cast<double>(cost_info.live_range));
    return (access_count * loop_factor) / range_factor;
}

void RISCV::RegisterAllocator::GraphColoring::simplify_phase(std::stack<std::string>& simplify_stack, const size_t K) {
    std::set<std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode>> workload;
    for (const std::pair<std::string, std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode>> pair : interference_graph)
        if (!pair.second->is_spilled && !pair.second->is_colored)
            workload.insert(pair.second);
    bool changed = true;
    while (changed) {
        changed = false;
        for (std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> node : workload) {
            if (node->degree() < K && node->move_related_neighbors.empty()) {
                // log_debug("Simplify variable %s", node->variable->name.c_str());
                simplify_stack.push(node->variable->name);
                for (std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> neighbor : node->non_move_related_neighbors)
                    neighbor->non_move_related_neighbors.erase(node);
                node->is_spilled = true;
                workload.erase(node);
                changed = true;
                break;
            }
        }
    }
}

bool RISCV::RegisterAllocator::GraphColoring::coalesce_phase(const size_t K) {
    for (const auto& [var_name, node] : interference_graph) {
        if (!node->is_spilled) {
            for (const auto& move_neighbor : node->move_related_neighbors) {
                std::string neighbor_name = move_neighbor->variable->name;
                if (can_coalesce_briggs(var_name, neighbor_name, K)) {
                    coalesce_nodes(var_name, neighbor_name);
                    return true;
                }
            }
        }
    }
    return false;
}

bool RISCV::RegisterAllocator::GraphColoring::freeze_phase(const size_t K) {
    for (const auto& [var_name, node] : interference_graph)
        if (!node->is_spilled && !node->is_colored && node->degree() < K && !node->move_related_neighbors.empty()) {
            for (std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> neighbor: node->move_related_neighbors)
                neighbor->move_related_neighbors.erase(node);
            node->move_related_neighbors.clear();
            log_debug("Freeze %s", node->variable->name.c_str());
            return true;
        }
    return false;
}

bool RISCV::RegisterAllocator::GraphColoring::spill_phase(std::stack<std::string>& simplify_stack, const size_t K) {
    std::string best_candidate = select_spill_candidate();
    if (!best_candidate.empty()) {
        simplify_stack.push(best_candidate);
        std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode>  node = interference_graph[best_candidate];
        node->is_spilled = true;
        for (std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> neighbor: node->move_related_neighbors)
            neighbor->move_related_neighbors.erase(node);
        for (std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> neighbor: node->non_move_related_neighbors)
            neighbor->non_move_related_neighbors.erase(node);
        log_debug("Select %s as spill candidate.", best_candidate.c_str());
        return true;
    }
    return false;
}

std::string RISCV::RegisterAllocator::GraphColoring::select_spill_candidate() {
    std::string best_candidate;
    double min_cost = std::numeric_limits<double>::max();
    for (const auto& [var_name, node] : interference_graph) {
        if (!node->is_spilled && !node->is_colored) {
            double cost = spill_costs[var_name].cost;
            if (cost < min_cost) {
                min_cost = cost;
                best_candidate = var_name;
            }
        }
    }
    return best_candidate;
}

template<typename StoreInst, typename LoadInst>
bool RISCV::RegisterAllocator::GraphColoring::assign_colors(std::stack<std::string> &stack) {
    for (const RISCV::Registers::ABI reg: available_colors) {
        std::set<std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode>> &colored_ = interference_graph[RISCV::Registers::to_string(reg)]->coalesced;
        for (std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> node : colored_) {
            node->is_colored = true;
            node->color = reg;
        }
    }
    while (!stack.empty()) {
        std::string var_name = stack.top();
        stack.pop();
        std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> &node = interference_graph[var_name];
        std::unordered_set<RISCV::Registers::ABI> used_colors;
        std::for_each(node->non_move_related_neighbors.begin(), node->non_move_related_neighbors.end(),
            [&](const std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> &neighbor) {
                if (neighbor->is_colored)
                    used_colors.insert(neighbor->color);
            }
        );
        std::vector<RISCV::Registers::ABI>::iterator color_iter = std::find_if(available_colors.begin(), available_colors.end(),
            [&used_colors](RISCV::Registers::ABI reg) {
                return used_colors.find(reg) == used_colors.end();
            }
        );
        if (color_iter != available_colors.end()) {
            node->color = *color_iter;
            node->is_colored = true;
            for (std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> coalesced : node->coalesced) {
                coalesced->is_colored = true;
                coalesced->color = *color_iter;
            }
        } else {
            log_debug("Marked %s for actual spilling", var_name.c_str());
            lir_function->spill<StoreInst, LoadInst>(node->variable);
            this->stack->add_variable(node->variable);
            build_interference_graph();
            while (!stack.empty()) stack.pop();
            return false;
        }
    }
    return true;
}

template bool RISCV::RegisterAllocator::GraphColoring::assign_colors<Backend::LIR::StoreFloat, Backend::LIR::LoadFloat>(std::stack<std::string> &stack);

bool RISCV::RegisterAllocator::GraphColoring::can_coalesce_briggs(const std::string& node1, const std::string& node2, const size_t K) {
    std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> n1 = interference_graph[node1];
    std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> n2 = interference_graph[node2];
    std::set<std::shared_ptr<InterferenceNode>> combined_neighbors;
    combined_neighbors.insert(n1->non_move_related_neighbors.begin(), n1->non_move_related_neighbors.end());
    combined_neighbors.insert(n2->non_move_related_neighbors.begin(), n2->non_move_related_neighbors.end());
    size_t high_degree_neighbors = 0;
    for (const std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> &neighbor : combined_neighbors)
        if (neighbor->non_move_related_neighbors.find(n1) != neighbor->non_move_related_neighbors.end() && neighbor->non_move_related_neighbors.find(n2) != neighbor->non_move_related_neighbors.end()) {
            if (neighbor->degree() >= K + 1)
                high_degree_neighbors++;
        } else if (neighbor->degree() >= K)
            high_degree_neighbors++;
    return high_degree_neighbors < K;
}

void RISCV::RegisterAllocator::GraphColoring::coalesce_nodes(const std::string& node1, const std::string& node2) {
    std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> n1 = interference_graph[node1];
    std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> n2 = interference_graph[node2];
    if (n2->is_colored) std::swap(n1, n2);
    *n1 += n2;
    log_debug("Coalesced %s and %s", n1->variable->name.c_str(), n2->variable->name.c_str());
    interference_graph.erase(n2->variable->name);
}
