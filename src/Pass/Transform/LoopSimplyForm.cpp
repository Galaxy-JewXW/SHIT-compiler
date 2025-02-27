#include <unordered_set>

#include "Mir/Builder.h"
#include "Pass/Analysis.h"
#include "Pass/Transform.h"

namespace Pass {
void LoopSimplyForm::transform(std::shared_ptr<Mir::Module> module) {
    const auto cfg_info = create<ControlFlowGraph>();
    const auto loop_info = create<LoopAnalysis>();
    cfg_info->run_on(module);
    module->update_id(); // DEBUG
    loop_info->run_on(module);

    for (auto &func: *module) {
        auto loops = loop_info->loops(func);
        auto block_predecessors = cfg_info->predecessors(func);
        auto block_dominators = cfg_info->dominator(func);
        //TODO:以下为了分割三种动作的逻辑，拆分出了三个循环，之后需要把这个架构重构一下

        for (auto &loop: loops) {
            //首先进行 entering 单一化：对于 header, 如果存在多个 entering, 则新建一个 , 将所有 entering 的跳转都指向它
            auto predecessors = block_predecessors[loop->header];
            std::vector<std::shared_ptr<Mir::Block>> entering;
            for (auto &predecessor: predecessors) {
                if (block_dominators[predecessor].find(loop->header) == block_dominators[loop->header].end()) entering.
                        push_back(predecessor);
            }

            if (entering.size() == 1) {
                loop->preheader = entering[0];
                continue;
            } // header 只有一个前驱，则为其 pre_header

            if (entering.empty()) {
                auto block = Mir::Block::create(Mir::Builder::gen_block_name(), func);
                auto jump_instruction = Mir::Jump::create(loop->header, block);
            } //循环位于 function 头部，或位于不可达处，需补上头节点

            if (entering.size() > 1) {
                auto pre_header = Mir::Block::create(Mir::Builder::gen_block_name(), func);
                auto jump_instruction = Mir::Jump::create(loop->header, pre_header);

                for (auto &enter: entering) {
                    enter->change_successor(loop->header, pre_header);
                } //先改变跳转关系

                //TODO: 这里本来还应该有 PHI 指令的前提操作，但因为中端翻译 while 指令的奇怪做法，目前认为 pre-header 的单一性被保证，暂时认为无需补足该方法
            } /*
                   *  多个 pre_header, 则新建一个 pre_header, 将所有 pre_header 的跳转都指向它
                   *  在将原来 header 节点中的 phi 指令转移到该 pre_header 中
                   * */
        }

        for (auto &loop: loops) {
            // loop 不会没有 latch 块，否则不被识别为循环
            if (loop->latch_blocks.size() == 1) {
                loop->latch = loop->latch_blocks[0];
                loop->latch_blocks.clear();
                continue;
            }
            //两步：改变跳转关系，header 与 latch 相关的 phi 指令移到 latch 中
            else {
                auto header = loop->header;

                auto latch_block = Mir::Block::create(Mir::Builder::gen_block_name(), func);
                auto jump_instruction = Mir::Jump::create(header, latch_block);

                for (auto &latch: loop->latch_blocks) {
                    latch->change_successor(header, latch_block);
                }

                auto phis = header->get_phis();
                for (auto &phi_: *phis) {
                    auto phi = std::dynamic_pointer_cast<Mir::Phi>(phi_);
                    Mir::Phi::Optional_Values values;
                    for (auto &latch: loop->latch_blocks) {
                        values[latch] = phi->get_optional_values()[latch];
                        phi->delete_optional_value(latch);
                    }
                    auto new_phi = Mir::Phi::create(phi->get_name(), phi->get_type(), latch_block, values);
                    phi->set_optional_value(latch_block, new_phi);
                }
            }
        }
    }
}
}
