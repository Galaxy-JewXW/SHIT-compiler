#ifndef RV_REGISTER_ALLOCATOR_GRAPH_COLORING_H
#define RV_REGISTER_ALLOCATOR_GRAPH_COLORING_H

#include <algorithm>
#include <set>
#include <stack>
#include <map>
#include <string>
#include <ostream>
#include "Backend/InstructionSets/RISC-V/RegisterAllocator/RegisterAllocator.h"
#include "Backend/LIR/Instructions.h"
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Backend/LIR/LIR.h"
#include "Backend/VariableTypes.h"
#include "Utils/Log.h"

#define BLOCK_ENTRY "block_entry"

namespace RISCV::RegisterAllocator {
    class FGraphColoring;
}

class RISCV::RegisterAllocator::GraphColoring : public RISCV::RegisterAllocator::Allocator {
    public:
        explicit GraphColoring(const std::shared_ptr<Backend::LIR::Function> &function, const std::shared_ptr<RISCV::Stack> &stack) : Allocator(function, stack) {};
        ~GraphColoring() override = default;
        virtual void allocate() override;
    private:
        std::shared_ptr<RISCV::RegisterAllocator::FGraphColoring> float_allocator;
    protected:
        bool (*is_consistent)(const Backend::VariableType &type) = Backend::Utils::is_int;

        class InterferenceNode : public std::enable_shared_from_this<InterferenceNode> {
            public:
                std::shared_ptr<Backend::Variable> variable;
                std::set<std::shared_ptr<InterferenceNode>> move_related_neighbors;
                std::set<std::shared_ptr<InterferenceNode>> non_move_related_neighbors;
                std::set<std::shared_ptr<InterferenceNode>> coalesced;
                bool is_spilled{false};
                bool is_colored{false};
                RISCV::Registers::ABI color{RISCV::Registers::ABI::ZERO};

                explicit InterferenceNode(const std::shared_ptr<Backend::Variable> &var) : variable(var) {}
                explicit InterferenceNode(RISCV::Registers::ABI reg) : variable(nullptr), is_colored(true), color(reg) {}
                [[nodiscard]] inline size_t degree() const { return non_move_related_neighbors.size(); }

                InterferenceNode& operator+=(const std::shared_ptr<InterferenceNode> &other) {
                    coalesced.insert(other);
                    coalesced.insert(other->coalesced.begin(), other->coalesced.end());
                    move_related_neighbors.erase(other);
                    for (std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> move_neighbor : other->move_related_neighbors) {
                        if (move_neighbor != shared_from_this()) {
                            move_neighbor->move_related_neighbors.erase(other);
                            move_neighbor->move_related_neighbors.insert(shared_from_this());
                            move_related_neighbors.insert(move_neighbor);
                        }
                    }
                    for (std::shared_ptr<RISCV::RegisterAllocator::GraphColoring::InterferenceNode> non_move_neighbor : other->non_move_related_neighbors) {
                        move_related_neighbors.erase(non_move_neighbor);
                        non_move_related_neighbors.insert(non_move_neighbor);
                        non_move_neighbor->move_related_neighbors.erase(shared_from_this());
                        non_move_neighbor->non_move_related_neighbors.erase(other);
                        non_move_neighbor->non_move_related_neighbors.insert(shared_from_this());
                    }
                    return *this;
                }
        };

        struct SpillCost {
            double cost{0.0};
            int use_count{0};
            int def_count{0};
            int loop_depth{0};
            int live_range{0};
        };

        std::unordered_map<std::string, std::shared_ptr<InterferenceNode>> interference_graph;
        std::unordered_map<std::string, SpillCost> spill_costs;
        std::vector<RISCV::Registers::ABI> available_colors;

        void __allocate__();
        void create_entry();
        // Create variable for physical registers and insert `move` instructions for parameters.
        virtual void create_registers();
        // Create nodes for variables stored in registers and physical registers.
        void create_interference_nodes(const std::vector<RISCV::Registers::ABI> &registers);
        virtual void build_interference_graph();
        template <size_t N>
        void build_interference_graph(const std::array<RISCV::Registers::ABI, N> &caller_saved);
        void print_interference_graph();
        void calculate_spill_costs();
        double calculate_spill_cost(const RISCV::RegisterAllocator::GraphColoring::SpillCost &cost_info);

        bool can_coalesce_briggs(const std::string& node1, const std::string& node2, const size_t K);
        void coalesce_nodes(const std::string& node1, const std::string& node2);

        void simplify_phase(std::stack<std::string>& simplify_stack, const size_t K);
        bool coalesce_phase(const size_t K);
        bool freeze_phase(const size_t K);
        bool spill_phase(std::stack<std::string>& simplify_stack, const size_t K);
        std::string select_spill_candidate();

        template<typename StoreInst, typename LoadInst>
        bool assign_colors(std::stack<std::string> &stack);
};

#endif