#ifndef RV_REGISTER_ALLOCATOR_H
#define RV_REGISTER_ALLOCATOR_H

#include <memory>
#include <string>
#include <map>
#include <vector>
#include "Mir/Instruction.h"
#include "Backend/InstructionSets/RISC-V/Modules/Modules.h"
#include "Backend/InstructionSets/RISC-V/Registers.h"
#include "Utils/Log.h"

namespace RISCV::RegisterAllocator {
    enum class AllocStrategy {
        LINEAR,
        GRAPH_COLOR
    };

    const static AllocStrategy CUR_STRATEGY = AllocStrategy::LINEAR;

    class Base {
        public:
            virtual ~Base() = default;

            // virtual void analyse(RISCV::Modules::Function& function) = 0;

            static std::unique_ptr<Base> create(AllocStrategy strategy);

            virtual std::string to_string() const = 0;
    };

    class LinearAllocator : public Base {
        public:
            // std::shared_ptr<Backend::MIR::Variable> register_status[RISCV::Registers::__ALL_REGS__]{};

//             void analyse(RISCV::Modules::Function& function) override;

            [[nodiscard]] std::string to_string() const override { return "Linear"; }
    };

    // class GraphColoringAllocator : public Base {
    //     public:
    //         void allocate(const std::shared_ptr<Mir::Function>& function, RISCV::Modules::FunctionField& function_field) override;

    //         AllocStrategy getStrategy() const override {
    //             return AllocStrategy::GRAPH_COLOR;
    //         }

    //     private:
    //         struct LiveInterval {
    //             std::string var;
    //             int start;
    //             int end;
    //         };

    //         struct Edge {
    //             std::string from;
    //             std::string to;

    //             bool operator<(const Edge& other) const {
    //                 if (from != other.from)
    //                     return from < other.from;
    //                 return to < other.to;
    //             }

    //             bool operator==(const Edge& other) const {
    //                 return (from == other.from && to == other.to) || (from == other.to && to == other.from);
    //             }
    //         };

    //         std::vector<LiveInterval> computeLiveIntervals(const std::shared_ptr<Mir::Function>& function);

    //         std::map<std::string, std::set<std::string>> buildInterferenceGraph(const std::vector<LiveInterval>& intervals);

    //         std::map<std::string, RISCV::Registers::ABI> colorGraph(const std::map<std::string, std::set<std::string>>& graph);
    // };
}

#endif