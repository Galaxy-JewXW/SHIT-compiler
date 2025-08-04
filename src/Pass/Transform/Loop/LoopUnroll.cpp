#include <unordered_set>

#include "Mir/Builder.h"
#include "Pass/Analysis.h"
#include "Pass/Transforms/Loop.h"
#include "Pass/Transforms/ControlFlow.h"
#include "Pass/Transforms/DataFlow.h"
#include "Pass/Analyses/SCEVAnalysis.h"
#include "Pass/Util.h"

namespace Pass {
    void LoopUnroll::transform(std::shared_ptr<Mir::Module> module) {
        this->cfg_info_ = get_analysis_result<ControlFlowGraph>(module);
        this->scev_info_ = get_analysis_result<SCEVAnalysis>(module);
        this->loop_info_ = get_analysis_result<LoopAnalysis>(module);
        for (auto &function : *module) {
            run_on( function);
        }
    }

    void LoopUnroll::transform(const std::shared_ptr<Mir::Function> &fun) {
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

    bool LoopUnroll::can_unroll(std::shared_ptr<LoopNodeTreeNode> loop_node, std::shared_ptr<Mir::Function> func) {
        if (loop_node->get_loop()->get_trip_count() > 0) return false;
        //循环遍历次数可以计算，则通过 const_loop_unroll 进行
        if (loop_node->get_loop()->get_exits().size() != 1)return false;
        auto exit_block = loop_node->get_loop()->get_exits()[0];
        auto &block_predecessors = this->cfg_info_->graph(func).predecessors.at(exit_block);
        if (block_predecessors.size() != 1 || *block_predecessors.begin() != loop_node->get_loop()->get_header()) return false;

        auto terminator = loop_node->get_loop()->get_header()->get_instructions().back();
        if (!terminator->is<Mir::Branch>()) return false;
        auto br = terminator->as<Mir::Branch>();
        auto cond = br->get_cond();
        if(!cond->is<Mir::Icmp>()) return false;
        auto icmp = cond->as<Mir::Icmp>();
        if (icmp->icmp_op() == Mir::Icmp::Op::EQ || icmp->icmp_op() == Mir::Icmp::Op::NE) return false;

        if(this->scev_info_->query(icmp->get_lhs()) && icmp->get_rhs()->is_constant()) {
            if(!this->scev_info_->query(icmp->get_lhs())->not_negative()) return false;
            this->init_num = this->scev_info_->query(icmp->get_lhs())->get_init();
            this->step_num = this->scev_info_->query(icmp->get_lhs())->get_step();
            if (this->step_num == 0) return false;
            int loop_size = loop_node->get_instr_size();
            return loop_size * this->unroll_times <= this->max_line_num;
        }
        else if(this->scev_info_->query(icmp->get_rhs()) && icmp->get_lhs()->is_constant()) {
            if(!this->scev_info_->query(icmp->get_rhs())->not_negative()) return false;
            this->init_num = this->scev_info_->query(icmp->get_rhs())->get_init();
            this->step_num = this->scev_info_->query(icmp->get_rhs())->get_step();
            if (this->step_num == 0) return false;
            int loop_size = loop_node->get_instr_size();
            return loop_size * this->unroll_times <= this->max_line_num;
        }
        return false;
    }

    bool LoopUnroll::try_unroll(std::shared_ptr<LoopNodeTreeNode> loop_node, std::shared_ptr<Mir::Function> func) {
        bool modified = false;
        for (auto child_node : loop_node->get_children()) modified |= try_unroll(child_node, func);

        if (loop_node->get_children().size() != 0) return modified; // 此时只考虑没有 child 的循环
        if (!can_unroll(loop_node, func)) return modified;
        //if can unroll

        for (auto phi : *loop_node->get_loop()->get_header()->get_phis()) {
            auto phi_instr = phi->as<Mir::Phi>();
            if (phi_instr->get_value_by_block(loop_node->get_loop()->get_latch()) == phi_instr) {
                phi_instr->replace_by_new_value(phi_instr->get_value_by_block(loop_node->get_loop()->get_preheader()));
                phi_instr->clear_operands();
            }
        }

        std::vector<std::shared_ptr<LoopNodeClone>> clone_infos;
        for (int i = 0; i <= this->unroll_times;i++) {
            auto loop_clone_info = loop_node->clone_loop_node();
            clone_infos.push_back(loop_clone_info);
            if (loop_node->get_parent()) {
                for(auto cpy_block : loop_clone_info->node_cpy->get_loop()->get_blocks()) {
                    loop_node->get_parent()->get_loop()->add_block(cpy_block);
                }
                for (auto child_node : loop_clone_info->node_cpy->get_children()) {
                    loop_node->get_parent()->add_child(child_node);
                }
                // 目前还不会处理有子循环的循环
            }
        }
        auto pre_header = loop_node->get_loop()->get_preheader();
        pre_header->get_instructions().back()->modify_operand(loop_node->get_loop()->get_header(), clone_infos[0]->node_cpy->get_loop()->get_header());

        auto begin_info = clone_infos[0];
        auto remainder_info = loop_node->clone_loop_node();
        begin_info->node_cpy->get_loop()->get_header()->get_instructions().back()->modify_operand(loop_node->get_loop()->get_exits()[0], remainder_info->node_cpy->get_loop()->get_header());
        for (auto phi : *loop_node->get_loop()->get_header()->get_phis()) {
            auto phi_instr = phi->as<Mir::Phi>();
            auto reflect_phi = remainder_info->get_value_reflect(phi_instr)->as<Mir::Phi>();
            reflect_phi->remove_optional_value(pre_header);
            reflect_phi->set_optional_value(begin_info->node_cpy->get_loop()->get_header(), begin_info->get_value_reflect(phi_instr));
        }

        auto begin_terminator = begin_info->node_cpy->get_loop()->get_header()->get_instructions().back();
        auto begin_branch = begin_terminator->as<Mir::Branch>();
        auto begin_icmp = begin_branch->get_cond()->as<Mir::Icmp>();
        auto op1 = begin_icmp->get_lhs();
        auto op2 = begin_icmp->get_rhs();
        auto icmp_next = *std::next(Utils::inst_as_iter(begin_icmp).value());
        auto sub_instr = Mir::Sub::create("unroll_sub", op2, Mir::ConstInt::create(init_num), begin_icmp->get_block());
        Utils::move_instruction_before(sub_instr, icmp_next);
        auto div_instr = Mir::Div::create("unroll_div", sub_instr, Mir::ConstInt::create(this->unroll_times * this->step_num), begin_icmp->get_block());
        Utils::move_instruction_before(div_instr, icmp_next);
        auto mul_instr = Mir::Mul::create("unroll_mul", div_instr, Mir::ConstInt::create(this->unroll_times * this->step_num), begin_icmp->get_block());
        Utils::move_instruction_before(mul_instr, icmp_next);
        auto add_instr = Mir::Add::create("unroll_add", mul_instr, Mir::ConstInt::create(this->init_num), begin_icmp->get_block());
        Utils::move_instruction_before(add_instr, icmp_next);
        auto add_instr2 = Mir::Add::create("unroll_add2", add_instr, Mir::ConstInt::create(-this->step_num), begin_icmp->get_block());
        Utils::move_instruction_before(add_instr2, icmp_next);
        auto icmp_instr = Mir::Icmp::create("unroll_icmp", begin_icmp->op, op1, add_instr2, begin_icmp->get_block());
        Utils::move_instruction_before(icmp_instr, icmp_next);
        begin_icmp->replace_by_new_value(icmp_instr);
        begin_icmp->clear_operands();

        //以下需要顺次处理所有出口块，但前置条件保证出口块唯一
        auto exit_block = loop_node->get_loop()->get_exits()[0];
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

        auto end_info = clone_infos[this->unroll_times - 1];
        auto termin = end_info->node_cpy->get_loop()->get_latch()->get_instructions().back();
        termin->clear_operands();
        termin->cleanup_users();
        auto jump_instr = Mir::Jump::create(begin_info->node_cpy->get_loop()->get_header(), end_info->node_cpy->get_loop()->get_latch());

        for (auto phi : *loop_node->get_loop()->get_header()->get_phis()) {
            auto phi_instr = phi->as<Mir::Phi>();
            auto value = phi_instr->get_value_by_block(loop_node->get_loop()->get_latch());
            auto new_phi = begin_info->get_value_reflect(phi_instr)->as<Mir::Phi>();
            new_phi->set_optional_value(end_info->node_cpy->get_loop()->get_latch(),end_info->get_value_reflect(value));
            new_phi->remove_optional_value(begin_info->node_cpy->get_loop()->get_latch());
        }
        for (int i = 1;i <= this->unroll_times;i++) {
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

            auto header_block = new_info->node_cpy->get_loop()->get_header();
            auto terminator = header_block->get_instructions().back();
            if (auto br = terminator->is<Mir::Branch>()) {
                auto next_block = (br->get_true_block() == loop_node->get_loop()->get_exits()[0]) ? br->get_false_block() : br->get_true_block();
                br->cleanup_users();
                br->clear_operands();
                auto new_jump_instruction = Mir::Jump::create(next_block, header_block);
            }
        }

        return true;
    }
}