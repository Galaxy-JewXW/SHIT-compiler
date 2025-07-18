#include "Mir/Builder.h"
#include "Pass/Analysis.h"
#include "Pass/Transforms/Loop.h"
#include "Pass/Transforms/ControlFlow.h"
#include "Pass/Transforms/DataFlow.h"

namespace Pass {

    void LoopUnSwitch::transform(std::shared_ptr<Mir::Module> module) {
        const auto loop_info = get_analysis_result<LoopAnalysis>(module);
        const auto cfg_info = get_analysis_result<ControlFlowGraph>(module);

        for (auto &fun: *module) {
            bool modified = true;
            while (modified) {
                modified = false;
                cfg_info->set_dirty(fun);

                // fixme: 这里关于 loop 的 Pass 都需要再检查正确性
                // fixme: 这里还没有关于 loop info 的更新
                create<LoopSimplyForm>()->run_on(fun);
                create<LCSSA>()->run_on(fun);

                for (auto node : loop_info->loop_forest(fun)) {
                    modified |= un_switching(node);
                }
                create<GlobalValueNumbering>()->run_on(fun);
                create<GlobalCodeMotion>()->run_on(fun);
                create<SimplifyControlFlow>()->run_on(fun);
            }
        }
    }

    bool LoopUnSwitch::un_switching(std::shared_ptr<LoopNodeTreeNode> &node) {
        auto loop = node->get_loop();
        if (std::find(this->un_switched_loops_.begin(), this->un_switched_loops_.end(), loop) == this->un_switched_loops_.end()) {
            return false;
        }
        for (auto child_node : node->get_children()) {
            if (un_switching(child_node)) return true;
        }

        std::vector<std::shared_ptr<Mir::Branch>> branch_vector;
        collect_branch(node, branch_vector);
        if (branch_vector.empty()) {
            this->un_switched_loops_.push_back(loop);
            return false;
        }

        handle_branch(node, branch_vector);
        return true;
    }

    void LoopUnSwitch::handle_branch(std::shared_ptr<LoopNodeTreeNode> &node, std::vector<std::shared_ptr<Mir::Branch>>& branch_vector) {
        auto branch = branch_vector[0];
        auto parent_function = branch->get_block()->get_function();
        auto last_preheader = node->get_loop()->get_preheader();

        std::vector<std::shared_ptr<Mir::Block>> true_blocks;
        std::vector<std::shared_ptr<Mir::Block>> false_blocks;
        for (auto br : branch_vector) {
            true_blocks.push_back(branch->get_true_block());
            false_blocks.push_back(branch->get_false_block());
        }

        std::vector<std::shared_ptr<Mir::Block>> cond_blocks;
        for (int i = 1;i < (1 << branch_vector.size()); i++) {
            cond_blocks.push_back(Mir::Block::create("cond_block", parent_function));
        }

        std::vector<std::shared_ptr<LoopNodeClone>> clone_infos;
        for (int i = (1 << branch_vector.size());i < (1 << (branch_vector.size() + 1)); i++) {
            auto new_info = node->clone_loop_node();
            clone_infos.push_back(new_info);
            auto new_node = new_info->node_cpy;

            for (int j = 0;j < branch_vector.size(); j++) {
                auto cond_instr = new_info->get_value_reflect(branch_vector[j])->as<Mir::Instruction>();
                auto cond_block = cond_instr->get_block();
                // fixme : cond_instr->delete_self()
                if((i & (1 << (branch_vector.size() - 1 - j))) == 0) {
                    Mir::Jump::create(new_info->get_value_reflect(true_blocks[j])->as<Mir::Block>(), cond_block);
                }
                else {
                    Mir::Jump::create(new_info->get_value_reflect(false_blocks[j])->as<Mir::Block>(), cond_block);
                }
            }
        }
    }

    void LoopUnSwitch::collect_branch(std::shared_ptr<LoopNodeTreeNode> &node, std::vector<std::shared_ptr<Mir::Branch>>& branch_vector) {
        auto loop = node->get_loop();
        for (auto& block : loop->get_blocks() ) {

            if (block->get_instructions().back()->is<Mir::Branch>()) {
                auto branch = block->get_instructions().back()->as<Mir::Branch>();
                if (branch->get_cond()->is<Mir::Const>()) continue;
                if (node->def_value(branch->get_cond())) continue;
                branch_vector.push_back(branch);
            }
        }

    }

}