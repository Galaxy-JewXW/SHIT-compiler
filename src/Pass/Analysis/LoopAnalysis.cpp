#include "Mir/Instruction.h"
#include "Pass/Analysis.h"

using FunctionPtr = std::shared_ptr<Mir::Function>;
using BlockPtr = std::shared_ptr<Mir::Block>;
namespace Pass {
    void LoopAnalysis::analyze(std::shared_ptr<const Mir::Module> module) {
        this->loops_.clear();
        std::shared_ptr<ControlFlowGraph> cfg_info = Pass::create<ControlFlowGraph>();
        std::shared_ptr<Mir::Module> mutable_module = std::const_pointer_cast<Mir::Module>(module);
        cfg_info->run_on(mutable_module);

        for (const auto &func : *module) {
            auto block_predecessors = cfg_info->predecessors(func);
            auto block_successors = cfg_info->successors(func);
            auto block_dominators = cfg_info->dominator(func);

            std::vector<std::shared_ptr<Mir::Block>> headers;
            for (const auto &block : func->get_blocks()) {
                for (const auto &successor : block_successors[block]) {
                    if (block_dominators[block].find(successor) != block_dominators[block].end()) {
                        headers.push_back(successor);
                    }
                }
            }// 每条回边对应一个头节点，每个头节点对应一个循环

            for (const auto &header_block : headers) {
                std::vector<BlockPtr> latching_blocks;
                for (const auto &predecessor : block_predecessors[header_block]) {
                    if (block_dominators[predecessor].find(header_block) != block_dominators[predecessor].end()) {
                        latching_blocks.push_back(predecessor);
                    }
                } //确认 latching_blocks,接下来 latching 节点的支配节点组成该循环

                std::vector<BlockPtr> working_set;
                std::vector<BlockPtr> loop_blocks;
                for (const auto &latching_block : latching_blocks) {
                    working_set.push_back(latching_block);
                    loop_blocks.push_back(latching_block);
                }//将 latch 节点加入循环，并使用工作集算法，寻找其支配结点，直至 header
                while (!working_set.empty()) {
                    auto current_block = working_set.back();
                    working_set.pop_back();
                    if (current_block != header_block) {
                        for (const auto &predecessor : block_predecessors[current_block]) {
                            if (std::find(loop_blocks.begin(), loop_blocks.end(), predecessor) == loop_blocks.end()) {
                                working_set.push_back(predecessor);
                                loop_blocks.push_back(predecessor);
                            }
                        }
                    }
                }//将结点的所有前驱结点加入循环结点集（自然循环中，循环所有结点被 header 支配）

                loops_[func].push_back(std::make_shared<Loop>(Loop{
                        .header = header_block,
                        .blocks = loop_blocks,
                        .latch_blocks = latching_blocks,
                }));
            }

            std::ostringstream oss;
            oss << "\n▷▷ loops in func: ["
                << func->get_name() << "]\n";
            for (const auto &loop: loops_[func]) {

                oss << "  ■ header: \"" << loop->header->get_name() << "\"\n";
                    for (const auto &block : loop->blocks) {
                        oss << "    block: \"" << block->get_name() << "\"\n";
                    }
                    for (const auto &block : loop->latch_blocks) {
                        oss << "    latch: \"" << block->get_name() << "\"\n";
                    }
            }
            log_debug("%s", oss.str().c_str());

        }
    }

}