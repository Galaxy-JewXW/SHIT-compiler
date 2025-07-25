#ifndef BRANCHPROBABILITYANALYSIS_H
#define BRANCHPROBABILITYANALYSIS_H

#include <unordered_set>

#include "Pass/Analysis.h"

namespace Pass {
class BranchProbabilityAnalysis final : public Analysis {
public:
    struct Edge {
        const Mir::Block *src;
        const Mir::Block *dst;
        int weight;

        struct Hash {
            std::size_t operator()(const Edge &e) const {
                return std::hash<const Mir::Block *>()(e.src) ^ std::hash<const Mir::Block *>()(e.dst) << 1;
            }
        };

    private:
        Edge(const Mir::Block *src, const Mir::Block *dst) :
            src(src), dst(dst), weight(0) {}

    public:
        bool operator==(const Edge &other) const {
            return src == other.src && dst == other.dst;
        }

        static Edge &make_edge(const Mir::Block *src, const Mir::Block *dst) {
            static std::unordered_set<Edge, Hash> edge_pool;
            const auto result = edge_pool.insert({src, dst});
            return const_cast<Edge &>(*result.first);
        }

        static Edge &make_edge(const std::shared_ptr<Mir::Block> &src, const std::shared_ptr<Mir::Block> &dst) {
            return make_edge(src.get(), dst.get());
        }
    };

private:
    std::unordered_map<const Mir::Function *, std::unordered_map<Edge, double, Edge::Hash>> edge_probabilities;

    std::unordered_map<const Mir::Function *, std::unordered_map<const Mir::Block *, double>> block_probabilities;

public:
    explicit BranchProbabilityAnalysis() :
        Analysis("BranchProbabilityAnalysis") {}

protected:
    void analyze(std::shared_ptr<const Mir::Module> module) override;
};
}

#endif //BRANCHPROBABILITYANALYSIS_H
