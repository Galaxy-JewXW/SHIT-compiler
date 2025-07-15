#include "Backend/InstructionSets/RISC-V/RegisterAllocator/GraphColoring.h"
#include "Backend/InstructionSets/RISC-V/Modules.h"
#include "Utils/Log.h"
#include <algorithm>
#include <limits>
#include <set>

void RISCV::RegisterAllocator::GraphColoring::allocate() {
    available_colors.insert(available_colors.end(), available_integer_regs.begin(), available_integer_regs.end());
    const int K = available_colors.size();
    build_interference_graph();
    while (true) {
        std::stack<std::string> simplify_stack;
        simplify_phase(simplify_stack, K);
        if (coalesce_phase(K)) continue;
        else if (freeze_phase(K)) continue;
        else if (spill_phase(simplify_stack, K)) continue;
        else if (assign_colors(simplify_stack)) continue;
        else break;
    }
    for (const auto& [var_name, node] : interference_graph) {
        if (node->is_colored && node->color != RISCV::Registers::ABI::ZERO) {
            var_to_reg[var_name] = node->color;
        } else {
            stack->add_variable(node->variable);
        }
    }
}

void RISCV::RegisterAllocator::GraphColoring::build_interference_graph() {
    interference_graph.clear();
    for (const auto& [var_name, var] : mir_function->variables) {
        if (var->lifetime != Backend::VariableWide::GLOBAL && var->lifetime != Backend::VariableWide::FUNCTION) {
            interference_graph[var_name] = std::make_shared<InterferenceNode>(var);
        }
    }
    // handle parameters
    for (const std::shared_ptr<Backend::Variable> &param : mir_function->parameters) {
        for (const std::shared_ptr<Backend::Variable> &param_ : mir_function->parameters) {
            if (param != param_) {
                interference_graph[param->name]->non_move_related_neighbors.insert(interference_graph[param_->name]);
                interference_graph[param_->name]->non_move_related_neighbors.insert(interference_graph[param->name]);
            }
        }
    }
    for (const std::shared_ptr<Backend::LIR::Block> &block : mir_function->blocks) {
        std::unordered_set<std::shared_ptr<Backend::Variable>> live = block->live_out;
        for (std::vector<std::shared_ptr<Backend::LIR::Instruction>>::reverse_iterator it = block->instructions.rbegin(); it != block->instructions.rend(); it++) {
            const std::shared_ptr<Backend::LIR::Instruction> instr = *it;
            std::shared_ptr<Backend::Variable> def_var = instr->get_defined_variable();
            std::vector<std::shared_ptr<Backend::Variable>> used_vars = instr->get_used_variables();
            bool is_move_instruction = (def_var != nullptr && used_vars.size() == 1 && instr->type == Backend::LIR::InstructionType::MOVE);
            if (def_var) {
                for (const std::shared_ptr<Backend::Variable> &live_var : live) {
                    if (live_var != def_var) {
                        if (is_move_instruction && std::find(used_vars.begin(), used_vars.end(), live_var) != used_vars.end()) {
                            interference_graph[def_var->name]->move_related_neighbors.insert(interference_graph[live_var->name]);
                            interference_graph[live_var->name]->move_related_neighbors.insert(interference_graph[def_var->name]);
                        } else {
                            interference_graph[def_var->name]->non_move_related_neighbors.insert(interference_graph[live_var->name]);
                            interference_graph[live_var->name]->non_move_related_neighbors.insert(interference_graph[def_var->name]);
                        }
                    }
                }
                live.erase(def_var);
            }
            for (const std::shared_ptr<Backend::Variable> &used_var : used_vars) {
                live.insert(used_var);
            }
        }
    }
}

void RISCV::RegisterAllocator::GraphColoring::calculate_spill_costs() {
    spill_costs.clear();
    for (const auto& [var_name, var] : mir_function->variables) {
        if (var->lifetime != Backend::VariableWide::GLOBAL && var->lifetime != Backend::VariableWide::FUNCTION) {
            spill_costs[var_name] = SpillCost{};
        }
    }
    int instr_idx = 0;
    std::unordered_map<std::string, int> first_use;
    std::unordered_map<std::string, int> last_use;
    for (const auto& block : mir_function->blocks) {
        for (const auto& instr : block->instructions) {
            auto def_var = instr->get_defined_variable();
            if (def_var && spill_costs.find(def_var->name) != spill_costs.end()) {
                spill_costs[def_var->name].def_count++;
                if (first_use.find(def_var->name) == first_use.end()) {
                    first_use[def_var->name] = instr_idx;
                }
                last_use[def_var->name] = instr_idx;
            }
            auto used_vars = instr->get_used_variables();
            for (const auto& used_var : used_vars) {
                if (spill_costs.find(used_var->name) != spill_costs.end()) {
                    spill_costs[used_var->name].use_count++;
                    if (first_use.find(used_var->name) == first_use.end()) {
                        first_use[used_var->name] = instr_idx;
                    }
                    last_use[used_var->name] = instr_idx;
                }
            }
            instr_idx++;
        }
    }
    for (auto& [var_name, cost] : spill_costs) {
        if (first_use.find(var_name) != first_use.end() && last_use.find(var_name) != last_use.end()) {
            cost.live_range = last_use[var_name] - first_use[var_name] + 1;
        }
        cost.loop_depth = 1; // TODO: Implement loop depth analysis
        cost.cost = calculate_spill_cost(var_name);
    }
}

double RISCV::RegisterAllocator::GraphColoring::calculate_spill_cost(const std::string &var_name) {
    const auto& cost_info = spill_costs[var_name];
    // cost = (use_count + def_count * 2) * pow(10, loop_depth) / live_range
    double access_count = cost_info.use_count + cost_info.def_count * 2.0;
    double loop_factor = std::pow(10.0, cost_info.loop_depth);
    double range_factor = std::max(1.0, static_cast<double>(cost_info.live_range));
    return (access_count * loop_factor) / range_factor;
}

bool RISCV::RegisterAllocator::GraphColoring::simplify_phase(std::stack<std::string>& simplify_stack, const int K) {
    std::vector<std::string> workload;
    for (const auto& [var_name, node] : interference_graph) {
        if (!node->is_spilled) {
            workload.push_back(var_name);
        }
    }
    bool changed;
    do {
        changed = false;
        for (const auto& var_name : workload) {
            std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> node = interference_graph[var_name];
            if (node->degree() < K && node->move_related_neighbors.empty()) {
                simplify_stack.push(var_name);
                for (std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> neighbor : node->move_related_neighbors)
                    neighbor->non_move_related_neighbors.erase(node);
                node->simplified = true;
                workload.erase(std::remove(workload.begin(), workload.end(), var_name), workload.end());
                changed = true;
                break;
            }
        }
    } while (changed);
}

bool RISCV::RegisterAllocator::GraphColoring::coalesce_phase(const int K) {
    for (const auto& [var_name, node] : interference_graph)
        if (!node->is_spilled && !node->simplified)
            for (const auto& move_neighbor : node->move_related_neighbors) {
                std::string neighbor_name = move_neighbor->variable->name;
                if (can_coalesce_briggs(var_name, neighbor_name, K)) {
                    coalesce_nodes(var_name, neighbor_name);
                    return true;
                }
            }
    return false;
}

bool RISCV::RegisterAllocator::GraphColoring::freeze_phase(const int K) {
    for (const auto& [var_name, node] : interference_graph)
        if (!node->is_spilled && !node->simplified)
            if (!node->move_related_neighbors.empty()) {
                std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> move_neighbor = *node->move_related_neighbors.begin();
                node->move_related_neighbors.erase(move_neighbor);
                move_neighbor->move_related_neighbors.erase(node);
                return true;
            }
    return false;
}

// potential spill
bool RISCV::RegisterAllocator::GraphColoring::spill_phase(std::stack<std::string>& simplify_stack, int K) {
    std::string best_candidate = select_spill_candidate();
    if (!best_candidate.empty()) {
        simplify_stack.push(best_candidate);
        auto node = interference_graph[best_candidate];
        node->is_spilled = true;
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

bool RISCV::RegisterAllocator::GraphColoring::assign_colors(std::stack<std::string> &stack) {
    while (!stack.empty()) {
        std::string var_name = stack.top();
        stack.pop();
        auto node = interference_graph[var_name];
        node->is_spilled = false;
        std::unordered_set<RISCV::Registers::ABI> used_colors;
        for (const std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> &neighbor : node->non_move_related_neighbors) {
            if (neighbor->is_colored) {
                used_colors.insert(neighbor->color);
            }
        }
        std::vector<RISCV::Registers::ABI>::iterator color_iter = std::find_if(available_colors.begin(), available_colors.end(),
            [&used_colors](RISCV::Registers::ABI reg) {
                return used_colors.find(reg) == used_colors.end();
            }
        );
        if (color_iter != available_colors.end()) {
            node->color = *color_iter;
            node->is_colored = true;
        } else {
            log_debug("No available color for variable %s, marked for actual spilling", var_name.c_str());
            insert_spill_code(node->variable);
            build_interference_graph();
            return false;
        }
    }
    return true;
}

void RISCV::RegisterAllocator::GraphColoring::insert_spill_code(std::shared_ptr<Backend::Variable> &var) {
    if (var->position != Backend::VariableWide::LOCAL && var->position != Backend::VariableWide::PARAMETER)
        log_error("Cannot spill variable %s, it is not a local variable", var->name.c_str());
    std::shared_ptr<Backend::LIR::LocalVariable> local_var = std::static_pointer_cast<Backend::LIR::LocalVariable>(var);
    mir_function->upgrade(local_var);
}

bool RISCV::RegisterAllocator::GraphColoring::can_coalesce_briggs(const std::string& node1, const std::string& node2, int K) {
    std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> n1 = interference_graph[node1];
    std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> n2 = interference_graph[node2];
    std::set<std::shared_ptr<InterferenceNode>> combined_neighbors;
    combined_neighbors.insert(n1->move_related_neighbors.begin(), n1->move_related_neighbors.end());
    combined_neighbors.insert(n1->non_move_related_neighbors.begin(), n1->non_move_related_neighbors.end());
    combined_neighbors.insert(n2->move_related_neighbors.begin(), n2->move_related_neighbors.end());
    combined_neighbors.insert(n2->non_move_related_neighbors.begin(), n2->non_move_related_neighbors.end());
    combined_neighbors.erase(n1);
    combined_neighbors.erase(n2);
    int high_degree_neighbors = 0;
    for (const auto& neighbor : combined_neighbors) {
        if (neighbor->degree() >= K) {
            high_degree_neighbors++;
        }
    }
    return high_degree_neighbors < K;
}

void RISCV::RegisterAllocator::GraphColoring::coalesce_nodes(const std::string& node1, const std::string& node2) {
    log_debug("Coalesced variables %s and %s using Briggs heuristic", node1.c_str(), node2.c_str());
    std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> n1 = interference_graph[node1];
    std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> n2 = interference_graph[node2];
    mir_function->remove_variable(n2->variable);
    n1->move_related_neighbors.insert(n2->move_related_neighbors.begin(), n2->move_related_neighbors.end());
    n1->move_related_neighbors.erase(n1);
    n1->move_related_neighbors.erase(n2);
    n1->non_move_related_neighbors.insert(n2->non_move_related_neighbors.begin(), n2->non_move_related_neighbors.end());
    interference_graph.erase(node2);
    for (auto& move_neighbor : n1->move_related_neighbors) {
        move_neighbor->move_related_neighbors.erase(n2);
        move_neighbor->move_related_neighbors.insert(n1);
    }
    for (auto& move_neighbor : n1->non_move_related_neighbors) {
        move_neighbor->move_related_neighbors.erase(n2);
        move_neighbor->move_related_neighbors.insert(n1);
    }
    mir_function->update(n2->variable, n1->variable);
}
