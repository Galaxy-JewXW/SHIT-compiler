#ifndef ALIASANALYSIS_H
#define ALIASANALYSIS_H

#include "Pass/Analyses/DominanceGraph.h"
#include "Pass/Analysis.h"

namespace Pass {
// 每个指针被赋予一组属性标识符，通过比较这些属性可以判断两个指针是否可能指向同一内存位置
class AliasAnalysis final : public Analysis {
public:
    struct Result {
        struct PairHasher {
            size_t operator()(const std::pair<size_t, size_t> &p) const {
                return std::hash<size_t>()(p.first) ^ std::hash<size_t>()(p.second) << 1;
            }
        };

        // 互不相交的属性对
        std::unordered_set<std::pair<size_t, size_t>, PairHasher> distinct_pairs;
        // 互不相交的属性组
        std::vector<std::unordered_set<size_t>> distinct_groups;
        // 将 LLVM 指针值映射到一组属性标识符上
        std::unordered_map<std::shared_ptr<Mir::Value>, std::vector<size_t>> pointer_attributes;

        // 两个别名属性id是相互排斥的
        void add_distinct_pair_id(const size_t &l, const size_t &r) {
            if (l == r) {
                log_error("Id %lu and %lu cannot be the same", l, r);
            }
            distinct_pairs.insert({l, r});
        }

        // 将 LLVM 指针值映射到一组属性标识符上
        void set_value_attrs(const std::shared_ptr<Mir::Value> &value, const std::vector<size_t> &attrs) {
            if (!value->get_type()->is_pointer()) {
                log_error("Value %s is not a pointer", value->to_string().c_str());
            }
            auto new_attrs = attrs;
            std::sort(new_attrs.begin(), new_attrs.end());
            pointer_attributes[value] = new_attrs;
        }

        bool add_value_attrs(const std::shared_ptr<Mir::Value> &value, const std::vector<size_t> &attrs) {
            if (attrs.empty()) {
                return false;
            }
            if (pointer_attributes.count(value) == 0) {
                pointer_attributes[value] = {};
            }
            auto &value_attrs = pointer_attributes[value];
            const auto old_size = value_attrs.size();
            value_attrs.insert(value_attrs.end(), attrs.begin(), attrs.end());
            value_attrs.erase(std::unique(value_attrs.begin(), value_attrs.end()), value_attrs.end());
            std::sort(value_attrs.begin(), value_attrs.end());
            return old_size != value_attrs.size();
        }

        bool add_value_attrs(const std::shared_ptr<Mir::Value> &value, const size_t attr) {
            return add_value_attrs(value, std::vector{attr});
        }

        void add_distinct_group(const std::unordered_set<size_t> &set) { distinct_groups.push_back(set); }

        std::vector<size_t> inherit_from(const std::shared_ptr<Mir::Value> &value) {
            return pointer_attributes.count(value) ? pointer_attributes[value] : std::vector<size_t>{};
        }
    };

    struct InheritEdge {
        std::shared_ptr<Mir::Value> dst, src1, src2;

        InheritEdge(const std::shared_ptr<Mir::Value> &dst, const std::shared_ptr<Mir::Value> &src1,
                    const std::shared_ptr<Mir::Value> &src2 = nullptr) : dst{dst}, src1{src1}, src2{src2} {}

        bool operator==(const InheritEdge &other) const {
            return dst == other.dst && src1 == other.src1 && src2 == other.src2;
        }

        bool operator==(InheritEdge &&other) const {
            return dst == other.dst && src1 == other.src1 && src2 == other.src2;
        }

        static constexpr size_t magic_num = 0x9e3779b9;

        struct Hasher {
            size_t operator()(const InheritEdge &edge) const {
                const size_t h1 = std::hash<std::shared_ptr<Mir::Value>>()(edge.dst),
                             h2 = std::hash<std::shared_ptr<Mir::Value>>()(edge.src1),
                             h3 = std::hash<std::shared_ptr<Mir::Value>>()(edge.src2);
                size_t seed = h1;
                seed ^= h2 + magic_num + (seed << 6) + (seed >> 2);
                seed ^= h3 + magic_num + (seed << 6) + (seed >> 2);
                return seed;
            }
        };
    };

    explicit AliasAnalysis() : Analysis("AliasAnalysis") {}

    void run_on_func(const std::shared_ptr<Mir::Function> &func);

    void analyze(std::shared_ptr<const Mir::Module> module) override;

private:
    std::shared_ptr<Mir::Module> module;

    std::shared_ptr<DominanceGraph> dom_graph{nullptr};

    std::vector<std::shared_ptr<Result>> results{};
};
} // namespace Pass

#endif // ALIASANALYSIS_H
