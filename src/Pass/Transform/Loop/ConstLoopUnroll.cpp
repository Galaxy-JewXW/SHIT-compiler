#include <unordered_set>

#include "Mir/Builder.h"
#include "Pass/Analysis.h"
#include "Pass/Transforms/Loop.h"
#include "Pass/Transforms/ControlFlow.h"
#include "Pass/Transforms/DataFlow.h"
#include "Pass/Analyses/SCEVAnalysis.h"
#include "Pass/Util.h"

namespace Pass {
    void ConstLoopUnroll::transform(std::shared_ptr<Mir::Module> module) {
        this->cfg_info_ = get_analysis_result<ControlFlowGraph>(module);
        this->scev_info_ = get_analysis_result<SCEVAnalysis>(module);
        this->loop_info_ = get_analysis_result<LoopAnalysis>(module);
        for (auto &function : *module) {
            run_on( function);
        }
    }

    void ConstLoopUnroll::transform(const std::shared_ptr<Mir::Function> &fun) {
        bool modified = true;
        while (modified) {
            modified = false;
            this->cfg_info_->set_dirty(fun);
            this->loop_info_->set_dirty(fun);
            create<LoopSimplyForm>()->run_on(fun);
            create<LCSSA>()->run_on(fun);
            create<LoopInvariantCodeMotion>()->run_on(fun);
            auto new_loop_info = get_analysis_result<LoopAnalysis>(Mir::Module::instance());
            for (auto node: new_loop_info->loop_forest(fun)) {
                modified |= try_unroll(node, fun);
            }
            create<GlobalValueNumbering>()->run_on(fun);
            create<SimplifyControlFlow>()->run_on(fun);
        }
    }

    bool ConstLoopUnroll::try_unroll(std::shared_ptr<LoopNodeTreeNode> loop_node, std::shared_ptr<Mir::Function> func) {
        bool modified = false;
        for (auto child_node : loop_node->get_children()) modified |= try_unroll(child_node, func);

        if (loop_node->get_children().size() != 0) return false; // 此时只考虑没有 child 的循环
        if (loop_node->get_loop()->get_trip_count() <=0) return modified;
        if (loop_node->get_loop()->get_exits().size() != 1) return modified;
        auto exit_block = loop_node->get_loop()->get_exits()[0];
        auto &block_predecessors = this->cfg_info_->graph(func).predecessors.at(exit_block);

        if (block_predecessors.size() != 1 || *block_predecessors.begin() != loop_node->get_loop()->get_header()) return modified;
        int loop_instr_size = loop_node->get_instr_size();
        if (loop_instr_size * loop_node->get_loop()->get_trip_count() > this->max_line_num) return modified;

        for (auto phi : *loop_node->get_loop()->get_header()->get_phis()) {
            auto phi_instr = phi->as<Mir::Phi>();
            if (phi_instr->get_value_by_block(loop_node->get_loop()->get_latch()) == phi_instr) {
                phi_instr->replace_by_new_value(phi_instr->get_value_by_block(loop_node->get_loop()->get_preheader()));
                phi_instr->clear_operands();
            }
        }

        std::vector<std::shared_ptr<LoopNodeClone>> clone_infos;
        for (int i = 0; i <= loop_node->get_loop()->get_trip_count();i++) {
            auto loop_clone_info = loop_node->clone_loop_node();
            clone_infos.push_back(loop_clone_info);
            if (loop_node->get_parent()) {
                for(auto cpy_block : loop_clone_info->node_cpy->get_loop()->get_blocks()) {
                    loop_node->get_parent()->get_loop()->add_block(cpy_block);
                }
                for (auto child_node : loop_clone_info->node_cpy->get_children()) {
                    loop_node->get_parent()->add_child(child_node);
                }
            }
        }
        auto pre_header = loop_node->get_loop()->get_preheader();
        pre_header->get_instructions().back()->modify_operand(loop_node->get_loop()->get_header(), clone_infos[0]->node_cpy->get_loop()->get_header());

        //以下需要顺次处理所有出口块，但前置条件保证出口块唯一
        for(auto phi : *exit_block->get_phis()) {
            auto phi_instr = phi->as<Mir::Phi>();
            for (auto pair : phi_instr->get_optional_values()) {
                if (clone_infos[0]->contain_value(pair.first)) {
                    auto new_key = clone_infos[loop_node->get_loop()->get_trip_count()]->get_value_reflect(pair.first);
                    if(clone_infos[0]->contain_value(pair.second)) {
                        auto new_value = clone_infos[loop_node->get_loop()->get_trip_count()]->get_value_reflect(pair.second);
                        phi_instr->remove_optional_value(pair.first);
                        phi_instr->set_optional_value(new_key->as<Mir::Block>(), new_value);
                    }
                    else {
                        phi_instr->remove_optional_value(pair.first);
                        phi_instr->set_optional_value(new_key->as<Mir::Block>(), pair.second);
                    }
                }
            }
        }//这里的 phi 种的键值对似乎在遍历中迭代了，后续可能需要修改

        auto begin_block = clone_infos[0]->node_cpy->get_loop()->get_header();
        for (auto phi : *begin_block->get_phis()) {
            auto phi_instr = phi->as<Mir::Phi>();
            auto value = phi_instr->get_value_by_block(loop_node->get_loop()->get_preheader());
            phi_instr->replace_by_new_value(value);
            phi_instr->clear_operands();
        }
        for (int i = 1;i <= loop_node->get_loop()->get_trip_count();i++) {
            auto new_info = clone_infos[i];
            auto pre_info = clone_infos[i - 1];

            auto pre_latch = pre_info->node_cpy->get_loop()->get_latch();
            auto pre_terminator = pre_latch->get_instructions().back();
            pre_latch->get_instructions().erase(Utils::inst_as_iter(pre_terminator).value());
            //这里本意是删除这条指令 delete value
            auto jump_instruction = Mir::Jump::create(new_info->node_cpy->get_loop()->get_header(), pre_latch);

            for(auto phi : *loop_node->get_loop()->get_header()->get_phis()) {
                auto phi_instr = phi->as<Mir::Phi>();
                auto value = phi_instr->get_value_by_block(loop_node->get_loop()->get_latch());
                auto reflect_phi = new_info->get_value_reflect(loop_node->get_loop()->get_latch())->as<Mir::Phi>();
                reflect_phi->replace_by_new_value(pre_info->get_value_reflect(value));
                reflect_phi->clear_operands();
                //这里本意也是删除这条指令 delete value
            }
        }

        for(int i = 0;i < loop_node->get_loop()->get_trip_count();i++) {
            auto new_info = clone_infos[i];
            auto header_block = new_info->node_cpy->get_loop()->get_header();
            auto terminator = header_block->get_instructions().back();
            if (auto br = terminator->is<Mir::Branch>()) {
                auto next_block = (br->get_true_block() == loop_node->get_loop()->get_exits()[0]) ? br->get_false_block() : br->get_true_block();
                br->cleanup_users();
                br->clear_operands();
                auto jump_instruction = Mir::Jump::create(next_block, header_block);
            }
        }

        return true;
    }
}