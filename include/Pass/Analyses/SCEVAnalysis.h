#ifndef SCEVANALYSIS_H
#define SCEVANALYSIS_H

#include "Pass/Analysis.h"
#include "Pass/Transforms/Loop.h"

namespace Pass {
    class SCEVExpr;
    class SCEVAnalysis final : public Analysis {
        using SCEVINFO = std::unordered_map<std::shared_ptr<Mir::Value>, std::shared_ptr<SCEVExpr>>;
    public:
        explicit SCEVAnalysis() : Analysis("SCEVAnalysis") {}

        void analyze(std::shared_ptr<const Mir::Module> module) override;

        SCEVINFO & get_SCEVinfo() { return SCEVinfo;}

        std::shared_ptr<SCEVExpr> query(std::shared_ptr<Mir::Value> value);

        void addSCEV(std::shared_ptr<Mir::Value> value, std::shared_ptr<SCEVExpr> scev);

        bool in_same_loop(std::shared_ptr<SCEVExpr> lhs, std::shared_ptr<SCEVExpr> rhs);

    private:
        std::unordered_map<std::shared_ptr<Mir::Value>, std::shared_ptr<SCEVExpr>> SCEVinfo;

        std::shared_ptr<Loop> find_loop(std::shared_ptr<Mir::Block> block, std::shared_ptr<LoopAnalysis> loop_info);

        std::shared_ptr<Mir::Value> get_initial(std::shared_ptr<Mir::Phi> phi, std::shared_ptr<Loop> loop);
        std::shared_ptr<Mir::Value> get_next(std::shared_ptr<Mir::Phi> phi, std::shared_ptr<Loop> loop);

        std::shared_ptr<SCEVExpr> fold_add(std::shared_ptr<SCEVExpr> lhs, std::shared_ptr<SCEVExpr> rhs);
        std::shared_ptr<SCEVExpr> fold_mul(std::shared_ptr<SCEVExpr> lhs, std::shared_ptr<SCEVExpr> rhs);
    };

    class SCEVExpr final {

    public:
        enum class SCEVTYPE { Constant, AddRec };

        explicit SCEVExpr(int constant) {
            type = SCEVTYPE::Constant;
            this->constant = constant;
        }

        SCEVExpr() {
            type = SCEVTYPE::AddRec;
        }

        void add_operand(const std::shared_ptr<SCEVExpr> &operand) {
            operands.push_back(operand);
        }

        void set_loop(const std::shared_ptr<Loop>& loop) {
            this->_loop = loop;
        }

        std::shared_ptr<Loop>& get_loop() {
            return _loop;
        }

        [[nodiscard]] SCEVTYPE get_type() const {
            return type;
        }

        [[nodiscard]] int get_constant() const {
            return this->constant;
        }

        [[nodiscard]] std::vector<std::shared_ptr<SCEVExpr>>& get_operands() {
            return this->operands;
        }



    private:
        std::vector<std::shared_ptr<SCEVExpr>> operands;
        int constant;
        SCEVTYPE type;
        std::shared_ptr<Loop> _loop;

    };
}

#endif //SCEVANALYSIS_H
