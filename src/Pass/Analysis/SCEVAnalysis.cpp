# include "Pass/Analyses/SCEVAnalysis.h"
namespace Pass {

    void SCEVAnalysis::analyze(std::shared_ptr<const Mir::Module> module) {
        const auto loop_info = get_analysis_result<LoopAnalysis>(module);

        for (const auto &func: *module) {
            auto loop_forest = loop_info->loop_forest(func);

            // BIV Analysis
            for (auto& block : func->get_blocks()) {
                auto loop = find_loop(block, loop_forest);
                for(auto &inst : block->get_instructions()) {
                    if (auto phi = inst->is<Mir::Phi>()) {
                        if (phi->get_optional_values().size() == 2) {
                            //fixme: 参照 NEL 的写法，loop 为 null 时这里必须保证 phi 指令中 block 按序排列，否则需参考 CMMC 进行两次重试
                            // 这里的逻辑可以再验证下
                            auto initial_value = get_initial(phi, loop);
                            auto next_value = get_next(phi, loop);
                            if (next_value->is<Mir::Const>() || !next_value->is<Mir::IntBinary>()) return;

                            auto next_inst = next_value->as<Mir::IntBinary>();
                            if (next_inst->intbinary_op() != Mir::IntBinary::Op::ADD) return;
                            auto op1 = next_inst->get_lhs();
                            auto op2 = next_inst->get_rhs();
                            if (op1 == phi || op2 == phi) {
                                auto step = (op1 == phi) ? op2 : op1;
                                if (step->is<Mir::ConstInt>()) {
                                    auto init_scev = this->query(initial_value);
                                    auto step_scev = this->query(step);
                                    if(init_scev && step_scev) {
                                        auto scev = std::make_shared<SCEVExpr>();
                                        scev->add_operand(init_scev);
                                        scev->add_operand(step_scev);
                                        scev->set_loop(loop);
                                        this->addSCEV(phi, scev);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // GIV Analysis
            for (auto & block : func->get_blocks()) {
                for (auto & instr : block->get_instructions()) {
                    if (auto binary = instr->is<Mir::IntBinary>()) {
                        if (binary->intbinary_op() == Mir::IntBinary::Op::ADD) {
                            auto lhs = query(binary->get_lhs());
                            auto rhs = query(binary->get_rhs());
                            if (lhs && rhs) {
                                if (lhs->get_loop() && rhs->get_loop() && lhs->get_loop() != rhs->get_loop()) return;
                                auto scev = fold_add(lhs, rhs);
                                if (scev) this->addSCEV(binary, scev);
                            }
                        }
                        if (binary->intbinary_op() == Mir::IntBinary::Op::MUL) {
                            auto lhs = query(binary->get_lhs());
                            auto rhs = query(binary->get_rhs());
                            if (lhs && rhs) {
                                auto scev = fold_mul(lhs, rhs);
                                if (scev) this->addSCEV(binary, scev);
                            }
                        }
                    }

                }
            }

        }
    }

    std::shared_ptr<Loop> SCEVAnalysis::find_loop(std::shared_ptr<Mir::Block> block, std::vector<std::shared_ptr<LoopNodeTreeNode>> loop_forest) {
        //fixme: 这种写法虽然简洁，但是效率不高


        for (auto top_node : loop_forest) {
            if(auto node = loop_contains(top_node, block)) return node->get_loop();
        }
        return nullptr;
    }

    std::shared_ptr<LoopNodeTreeNode> SCEVAnalysis::loop_contains(std::shared_ptr<LoopNodeTreeNode> node, std::shared_ptr<Mir::Block> block) {
        for (auto child: node->get_children()) {
            if (auto child_node = loop_contains(child, block)) return child_node;
        }
        if (node->get_loop()->get_header() == block) return node;
        return nullptr;
    }

    std::shared_ptr<Mir::Value> SCEVAnalysis::get_initial(std::shared_ptr<Mir::Phi> phi, std::shared_ptr<Loop> loop) {
        if (loop == nullptr) return phi->get_optional_values().begin()->second;
        return phi->get_value_by_block(loop->get_preheader());
    }

    std::shared_ptr<Mir::Value> SCEVAnalysis::get_next(std::shared_ptr<Mir::Phi> phi, std::shared_ptr<Loop> loop) {
        if (loop == nullptr) return phi->get_optional_values().end()->second;
        return phi->get_value_by_block(loop->get_latch());
    }

    std::shared_ptr<SCEVExpr> SCEVAnalysis::query(std::shared_ptr<Mir::Value> value) {
        if (this->get_SCEVinfo().find(value) == this->get_SCEVinfo().end()) {
            if (value->is<Mir::ConstInt>()) {
                addSCEV(value,
                        std::make_shared<SCEVExpr>(std::get<int>(value->as<Mir::ConstInt>()->get_constant_value())));
            }
        }
        return this->get_SCEVinfo().find(value)->second;
    }

    void SCEVAnalysis::addSCEV(std::shared_ptr<Mir::Value> value, std::shared_ptr<SCEVExpr> scev) {
        this->get_SCEVinfo().emplace(value, scev);
    }


    std::shared_ptr<SCEVExpr> SCEVAnalysis::fold_add(std::shared_ptr<SCEVExpr> lhs, std::shared_ptr<SCEVExpr> rhs) {
        if (!lhs || !rhs) return nullptr;
        if (lhs->get_type() == SCEVExpr::SCEVTYPE::Constant && rhs->get_type() == SCEVExpr::SCEVTYPE::Constant) {
            int constant = lhs->get_constant() + rhs->get_constant();
            return std::make_shared<SCEVExpr>(constant);
        }

        if (lhs->get_type() == SCEVExpr::SCEVTYPE::AddRec && rhs->get_type() == SCEVExpr::SCEVTYPE::Constant) {
            auto base = lhs->get_operands()[0];
            auto step = lhs->get_operands()[1];
            auto new_base = fold_add(base, rhs);
            if(new_base) {
                auto scev = std::make_shared<SCEVExpr>();
                scev->add_operand(new_base);
                scev->add_operand(step);
                return scev;
            }
        }

        if (lhs->get_type() == SCEVExpr::SCEVTYPE::AddRec && rhs->get_type() == SCEVExpr::SCEVTYPE::AddRec && in_same_loop(lhs, rhs)) {
            std::vector<std::shared_ptr<SCEVExpr>> operands;
            int size = std::max(lhs->get_operands().size(), rhs->get_operands().size());
            for (int i = 0; i < size; i++) {
                auto l = i < lhs->get_operands().size() ? lhs->get_operands()[i] : nullptr;
                auto r = i < rhs->get_operands().size() ? rhs->get_operands()[i] : nullptr;
                if (l && r) {
                    auto new_operand = fold_add(l, r);
                    if (new_operand) operands.push_back(new_operand);
                    else return nullptr;
                }
                else if (l) operands.push_back(l);
                else if (r) operands.push_back(r);
            }
            auto scev = std::make_shared<SCEVExpr>();
            scev->set_loop(lhs->get_loop());
            for (auto & operand : operands) scev->add_operand(operand);
            return scev;
        }
    }

    std::shared_ptr<SCEVExpr> SCEVAnalysis::fold_mul(std::shared_ptr<SCEVExpr> lhs, std::shared_ptr<SCEVExpr> rhs) {
        if(!lhs || !rhs) return nullptr;
        if(lhs->get_type() == SCEVExpr::SCEVTYPE::Constant && rhs->get_type() == SCEVExpr::SCEVTYPE::Constant) {
            int constant = lhs->get_constant() * rhs->get_constant();
            return std::make_shared<SCEVExpr>(constant);
        }

        if (lhs->get_type() == SCEVExpr::SCEVTYPE::AddRec && rhs->get_type() == SCEVExpr::SCEVTYPE::Constant) {
            std::vector<std::shared_ptr<SCEVExpr>> operands;
            for(auto operand : lhs->get_operands()) {
                auto new_operand = fold_mul(operand, rhs);
                if(new_operand) operands.push_back(new_operand);
                else return nullptr;
            }

            auto scev = std::make_shared<SCEVExpr>();
            scev->set_loop(lhs->get_loop());
            for (auto & operand : operands) scev->add_operand(operand);
            return scev;
        }
    }

    bool SCEVAnalysis::in_same_loop(std::shared_ptr<SCEVExpr> lhs, std::shared_ptr<SCEVExpr> rhs) {
        return lhs->get_loop() == rhs->get_loop() && lhs->get_loop() != nullptr;
    }


}