#ifndef RV_REGISTER_ALLOCATOR_GRAPH_COLORING_H
#define RV_REGISTER_ALLOCATOR_GRAPH_COLORING_H

#include <algorithm>
#include <limits>
#include <set>
#include <stack>
#include <map>
#include <string>
#include <ostream>
#include "Backend/InstructionSets/RISC-V/RegisterAllocator/RegisterAllocator.h"
#include "Backend/LIR/Instructions.h"
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Backend/LIR/LIR.h"
#include "Utils/Log.h"

class RISCV::RegisterAllocator::GraphColoring : public RISCV::RegisterAllocator::Allocator {
    public:
        explicit GraphColoring(const std::shared_ptr<Backend::LIR::Function> &function, const std::shared_ptr<RISCV::Stack> &stack) : Allocator(function, stack) {}
        void allocate() override;
    private:
        class InterferenceNode : public std::enable_shared_from_this<InterferenceNode> {
            public:
                std::shared_ptr<Backend::Variable> variable;
                std::set<std::shared_ptr<InterferenceNode>> move_related_neighbors;
                std::set<std::shared_ptr<InterferenceNode>> non_move_related_neighbors;
                bool is_spilled{false};
                bool is_colored{false};
                RISCV::Registers::ABI color{RISCV::Registers::ABI::ZERO};

                explicit InterferenceNode(const std::shared_ptr<Backend::Variable> &var) : variable(var) {};
                explicit InterferenceNode(RISCV::Registers::ABI reg) : variable(nullptr), is_colored(true), color(reg) {};

                [[nodiscard]] inline size_t degree() const {
                    return move_related_neighbors.size() + non_move_related_neighbors.size();
                }
        };

        struct SpillCost {
            double cost{0.0};
            int use_count{0};
            int def_count{0};
            int loop_depth{0};
            int live_range{0};
        };

        struct SpillInfo {
            RISCV::Registers::ABI reg;
            std::shared_ptr<Backend::Variable> var;
            int64_t stack_offset;
        };

        std::unordered_map<std::string, std::shared_ptr<InterferenceNode>> interference_graph;
        std::unordered_map<std::string, SpillCost> spill_costs;
        std::vector<RISCV::Registers::ABI> available_colors;
        std::vector<RISCV::Registers::ABI> lru;
        std::vector<SpillInfo> call_spill_list;

        // Create variable for physical registers and insert `move` instructions for parameters.
        void create_registers();
        // Create nodes for variables stored in registers and physical registers.
        void create_interference_nodes();
        void build_interference_graph();
        void print_interference_graph();
        void calculate_spill_costs();
        double calculate_spill_cost(const std::string &var_name);

        bool can_coalesce_briggs(const std::string& node1, const std::string& node2, const size_t K);
        void coalesce_nodes(const std::string& node1, const std::string& node2);

        void simplify_phase(std::stack<std::string>& simplify_stack, const size_t K);
        bool coalesce_phase(const size_t K);
        bool freeze_phase(const size_t K);
        bool spill_phase(std::stack<std::string>& simplify_stack, const size_t K);

        std::string select_spill_candidate();

        bool assign_colors(std::stack<std::string> &stack);
};

#endif
