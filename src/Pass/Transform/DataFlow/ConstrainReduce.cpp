#include "Pass/Analyses/IntervalAnalysis.h"
#include "Pass/Transforms/Common.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
constexpr long max_depth = 200l;

class Constraint {
public:
    explicit Constraint(const long size = max_depth) :
        matrix(size, std::vector(size, infinity)), size(size) {
        for (long i = 0; i < size; ++i) {
            matrix[i][i] = 0;
        }
    }

    Constraint(const Constraint &other) = default;

    Constraint &operator=(const Constraint &other) {
        if (this == &other) {
            return *this;
        }
        size = other.size;
        matrix = other.matrix;
        return *this;
    }

    ~Constraint() = default;

    void propagate();

    void add_relation(long i, long j, Icmp::Op icmp_type);

    [[nodiscard]] std::optional<bool> deduce_relation(long i, long j, Icmp::Op icmp_type);

private:
    std::vector<std::vector<long>> matrix;
    long size;
    static constexpr long infinity = std::numeric_limits<long>::max();
};

void Constraint::propagate() {
    for (long k = 0; k < size; ++k) {
        for (long i = 0; i < size; ++i) {
            for (long j = 0; j < size; ++j) {
                if (matrix[i][k] != infinity && matrix[k][j] != infinity) {
                    matrix[i][j] = std::min(matrix[i][j], matrix[i][k] + matrix[k][j]);
                }
            }
        }
    }
}

void Constraint::add_relation(const long i, const long j, const Icmp::Op icmp_type) {
    switch (icmp_type) {
        case Icmp::Op::EQ: {
            matrix[i][j] = std::min(matrix[i][j], 0l);
            matrix[j][i] = std::max(matrix[j][i], 0l);
            break;
        }
        case Icmp::Op::LT: {
            matrix[i][j] = std::min(matrix[i][j], -1l);
            break;
        }
        case Icmp::Op::LE: {
            matrix[i][j] = std::min(matrix[i][j], 0l);
            break;
        }
        case Icmp::Op::GT: {
            matrix[j][i] = std::min(matrix[j][i], 1l);
            break;
        }
        case Icmp::Op::GE: {
            matrix[j][i] = std::min(matrix[j][i], 0l);
            break;
        }
        // ReSharper disable once CppDFAUnreachableCode
        default:
            break;
    }
}

std::optional<bool> Constraint::deduce_relation(const long i, const long j, const Icmp::Op icmp_type) {
    switch (icmp_type) {
        case Icmp::Op::EQ: {
            if (matrix[i][j] <= 0 && matrix[j][i] <= 0)
                return true;
            if (matrix[i][j] <= -1 || matrix[j][i] <= -1)
                return false;
            break;
        }
        case Icmp::Op::NE: {
            if (matrix[i][j] <= 0 && matrix[j][i] <= 0)
                return false;
            if (matrix[i][j] <= -1 || matrix[j][i] <= -1)
                return true;
            break;
        }
        case Icmp::Op::LT: {
            if (matrix[i][j] <= -1)
                return true;
            if (matrix[j][i] <= 0)
                return false;
            break;
        }
        case Icmp::Op::LE: {
            if (matrix[i][j] <= 0)
                return true;
            if (matrix[j][i] <= -1)
                return false;
            break;
        }
        case Icmp::Op::GT:
        case Icmp::Op::GE: {
            return deduce_relation(j, i, Icmp::swap_op(icmp_type));
        }
        default:
            break;
    }
    return std::nullopt;
}

template<typename T>
class Map final : public std::unordered_map<T, int> {
    int cnt_{0};

public:
    int &operator[](const T &key) {
        const auto it = this->find(key);
        if (it == this->end()) {
            this->insert({key, ++cnt_});
        }
        if (this->size() >= max_depth) {
            throw std::runtime_error("");
        }
        return std::unordered_map<T, int>::operator[](key);
    }

    int &cnt() { return cnt_; }
};

class BranchConstrainReduceImpl {
public:
    BranchConstrainReduceImpl(const std::shared_ptr<Function> &current_func,
                              const Pass::ControlFlowGraph::Graph &cfg_graph,
                              const Pass::DominanceGraph::Graph &dom_graph,
                              const std::shared_ptr<Pass::IntervalAnalysis> &interval) :
        current_func(current_func), cfg_graph(cfg_graph), dom_graph(dom_graph), interval(interval) {}

    bool impl();

private:
    const std::shared_ptr<Function> &current_func;
    const Pass::ControlFlowGraph::Graph &cfg_graph;
    const Pass::DominanceGraph::Graph &dom_graph;
    const std::shared_ptr<Pass::IntervalAnalysis> &interval;

    Map<std::shared_ptr<Value>> id_map{};
    std::unordered_map<std::shared_ptr<Block>, Constraint> constraint_map{};
    bool changed{false};

    template<typename T>
    Pass::IntervalAnalysis::IntervalSet<T> get_interval(const std::shared_ptr<Value> &value);

    void run_on_block(const std::shared_ptr<Block> &block, Constraint &constraint);
};

template<typename T>
Pass::IntervalAnalysis::IntervalSet<T> BranchConstrainReduceImpl::get_interval(const std::shared_ptr<Value> &value) {
    if (value->is_constant()) {
        const auto constant = value->as<Const>()->get_constant_value();
        return std::visit([](const auto c) {
            return Pass::IntervalAnalysis::IntervalSet<T>(c);
        }, constant);
    }
    const auto inst = value->is<Instruction>();
    if (inst == nullptr) [[unlikely]] {
        log_error("Invalid value: %s", value->to_string().c_str());
    }
    const auto ctx = interval->ctx_after(inst);
    return std::get<Pass::IntervalAnalysis::IntervalSet<T>>(ctx.get(inst));
}

template<typename T>
std::pair<T, T> interval_limit(const Pass::IntervalAnalysis::IntervalSet<T> &set) {
    const auto &intervals = set.intervals();
    T _min = Pass::numeric_limits_v<T>::infinity;
    T _max = Pass::numeric_limits_v<T>::neg_infinity;
    for (const auto &i: intervals) {
        _min = std::min(_min, i.lower);
        _max = std::max(_max, i.upper);
    }
    return {_min, _max};
}

template<typename T>
bool interval_hit(const Pass::IntervalAnalysis::IntervalSet<T> &set, T val) {
    const auto &intervals = set.intervals();
    return std::any_of(intervals.begin(), intervals.end(), [&val](const auto &i) {
        return i.lower <= val && val <= i.upper;
    });
}

void BranchConstrainReduceImpl::run_on_block(const std::shared_ptr<Block> &block, Constraint &constraint) {
    constraint_map[block] = constraint;

    for (const auto &inst: block->get_instructions()) {
        if (inst->get_op() != Operator::INTBINARY)
            continue;
        const auto intbinary = inst->as<IntBinary>();
        const auto lhs = intbinary->get_lhs(), rhs = intbinary->get_rhs();
        const auto lhs_range = get_interval<int>(lhs), rhs_range = get_interval<int>(rhs);
        const int intbinary_id = id_map[intbinary];
        switch (intbinary->intbinary_op()) {
            case IntBinary::Op::ADD: {
                // c = a + b
                if (!lhs->is_constant()) {
                    const int idx = id_map[lhs];
                    if (const auto [min, max] = interval_limit(rhs_range); min > 0) {
                        // b > 0  => c > a
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::GT);
                    } else if (min == 0) {
                        // b >= 0 => c >= a
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::GE);
                    } else if (max < 0) {
                        // b < 0 => c < a
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::LT);
                    } else if (max == 0) {
                        // b <= 0 => c <= a
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::LE);
                    }
                }
                if (!rhs->is_constant()) {
                    const int idx = id_map[rhs];
                    if (const auto [min, max] = interval_limit(lhs_range); min > 0) {
                        // a > 0 => c > b
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::GT);
                    } else if (min == 0) {
                        // a >= 0 => c >= b
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::GE);
                    } else if (max < 0) {
                        // a < 0 => c < b
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::LT);
                    } else if (max == 0) {
                        // a <= 0 => c <= b
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::LE);
                    }
                }
                break;
            }
            case IntBinary::Op::SUB: {
                // c = a - b;
                if (!lhs->is_constant()) {
                    const int idx = id_map[lhs];
                    if (const auto [min, max] = interval_limit(rhs_range); min > 0) {
                        // b > 0 => c < a
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::LT);
                    } else if (min == 0) {
                        // b >= 0 => c <= a
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::LE);
                    } else if (max < 0) {
                        // b < 0 => c > a
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::GT);
                    } else if (max == 0) {
                        // b <= 0 => c >= a
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::GE);
                    }
                }
                break;
            }
            case IntBinary::Op::MUL: {
                // c = a * b;
                if (!lhs->is_constant()) {
                    const int idx = id_map[lhs];
                    if (rhs->is_constant() && **rhs->as<ConstInt>() == 1) {
                        // c = a * 1 => c = a;
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::EQ);
                    }
                    const auto [lhs_range_min, lhs_range_max] = interval_limit(lhs_range);
                    if (const auto [rhs_range_min, rhs_range_max] = interval_limit(rhs_range);
                        rhs_range_min >= 2) {
                        // b >= 2
                        if (lhs_range_min >= 1) {
                            // a >= 1 => c >= 2a > a
                            constraint.add_relation(intbinary_id, idx, Icmp::Op::GT);
                        } else if (lhs_range_max <= -1) {
                            // a <= -1 => c <= 2a < a
                            constraint.add_relation(intbinary_id, idx, Icmp::Op::LT);
                        }
                    }
                }
                if (!rhs->is_constant()) {
                    const int idx = id_map[rhs];
                    if (lhs->is_constant() && **lhs->as<ConstInt>() == 1) {
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::EQ);
                    }
                    const auto [rhs_range_min, rhs_range_max] = interval_limit(rhs_range);
                    if (const auto [lhs_range_min, lhs_range_max] = interval_limit(lhs_range);
                        lhs_range_min >= 2) {
                        if (rhs_range_min >= 1) {
                            constraint.add_relation(intbinary_id, idx, Icmp::Op::GT);
                        } else if (rhs_range_max <= -1) {
                            constraint.add_relation(intbinary_id, idx, Icmp::Op::LT);
                        }
                    }
                }
                break;
            }
            case IntBinary::Op::DIV: {
                // c = a / b
                if (!lhs->is_constant()) {
                    const int idx = id_map[lhs];
                    if (rhs->is_constant() && **rhs->as<ConstInt>() == 1) {
                        // c = a / 1 => c = a;
                        constraint.add_relation(intbinary_id, idx, Icmp::Op::EQ);
                    }
                    const auto [lhs_range_min, lhs_range_max] = interval_limit(lhs_range);
                    const auto [rhs_range_min, rhs_range_max] = interval_limit(rhs_range);
                    if (rhs_range_min >= 2) {
                        if (lhs_range_min >= 0) {
                            // a >= 0, b >= 2 => c <= a/2 <= a (should consider a=0,1)
                            constraint.add_relation(intbinary_id, idx, Icmp::Op::LE);
                        } else if (lhs_range_max < 0) {
                            // a < 0, b >= 2 => c > a (e.g., -10/2=-5 > -10)
                            constraint.add_relation(intbinary_id, idx, Icmp::Op::GT);
                        }
                    }
                    if (rhs_range_min <= -2) {
                        if (lhs_range_min > 0) {
                            // a > 0, b <= -2 => c < 0 < a
                            constraint.add_relation(intbinary_id, idx, Icmp::Op::LT);
                        } else if (lhs_range_max < 0) {
                            // a < 0, b <= -2 => c > 0 > a
                            constraint.add_relation(intbinary_id, idx, Icmp::Op::GT);
                        }
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    constraint.propagate();
    if (const auto terminator{block->get_instructions().back()}; terminator->get_op() == Operator::BRANCH) {
        const auto branch = terminator->as<Branch>();
        if (const auto icmp = terminator->as<Branch>()->get_cond()->is<Icmp>()) {
            if (const auto &lhs = icmp->get_lhs(), &rhs = icmp->get_rhs();
                !lhs->is_constant() && !rhs->is_constant()) {
                const auto idx1 = id_map[lhs], idx2 = id_map[rhs];
                if (const auto res = constraint.deduce_relation(idx1, idx2, icmp->icmp_op())) {
                    block->get_instructions().pop_back();
                    changed = true;
                    if (res.value()) {
                        Jump::create(branch->get_true_block(), block);
                    } else {
                        Jump::create(branch->get_false_block(), block);
                    }
                }
            } else if (rhs->is_constant()) {
                const auto constant_rhs = **rhs->as<ConstInt>();
                const auto lhs_range = get_interval<int>(lhs);
                const auto [lhs_min, lhs_max] = interval_limit(lhs_range);
                std::shared_ptr<Block> target_block{nullptr};
                switch (icmp->icmp_op()) {
                    case Icmp::Op::EQ: {
                        if (lhs_min == constant_rhs && lhs_max == constant_rhs) {
                            target_block = branch->get_true_block();
                        } else if (!interval_hit(lhs_range, constant_rhs)) {
                            target_block = branch->get_false_block();
                        }
                        break;
                    }
                    case Icmp::Op::NE: {
                        if (lhs_min == constant_rhs && lhs_max == constant_rhs) {
                            target_block = branch->get_false_block();
                        } else if (!interval_hit(lhs_range, constant_rhs)) {
                            target_block = branch->get_true_block();
                        }
                    }
                    case Icmp::Op::LT: {
                        if (lhs_max < constant_rhs) {
                            target_block = branch->get_true_block();
                        } else if (lhs_min >= constant_rhs) {
                            target_block = branch->get_false_block();
                        }
                        break;
                    }
                    case Icmp::Op::LE: {
                        if (lhs_max <= constant_rhs) {
                            target_block = branch->get_true_block();
                        } else if (lhs_min > constant_rhs) {
                            target_block = branch->get_false_block();
                        }
                        break;
                    }
                    case Icmp::Op::GT: {
                        if (lhs_min > constant_rhs) {
                            target_block = branch->get_true_block();
                        } else if (lhs_max <= constant_rhs) {
                            target_block = branch->get_false_block();
                        }
                        break;
                    }
                    case Icmp::Op::GE: {
                        if (lhs_min >= constant_rhs) {
                            target_block = branch->get_true_block();
                        } else if (lhs_max < constant_rhs) {
                            target_block = branch->get_false_block();
                        }
                        break;
                    }
                    default:
                        break;
                }
                if (target_block) {
                    block->get_instructions().pop_back();
                    changed = true;
                    Jump::create(target_block, block);
                }
            }
        }
    } else if (terminator->get_op() == Operator::SWITCH) {
        if (const auto switch_ = terminator->as<Switch>(); switch_->get_base()->get_type()->is_int32()) {
            std::vector<std::shared_ptr<ConstInt>> case_to_remove;
            case_to_remove.reserve(switch_->cases().size());
            const auto base_interval = get_interval<int>(switch_->get_base());
            for (const auto &[v, b]: switch_->cases()) {
                if (const auto c = v->as<ConstInt>(); !interval_hit(base_interval, **c)) {
                    case_to_remove.push_back(c);
                }
            }
            if (!case_to_remove.empty()) {
                changed = true;
            }
            for (const auto &c: case_to_remove) {
                switch_->remove_case(c);
            }
            if (switch_->cases().empty()) {
                block->get_instructions().pop_back();
                Jump::create(switch_->get_default_block(), block);
            }
        }
    }

    for (const auto &child: dom_graph.dominance_children.at(block)) {
        auto child_constraint = constraint;
        if (const auto terminator = block->get_instructions().back();
            terminator->get_op() == Operator::BRANCH) {
            const auto branch = terminator->as<Branch>();
            if (const auto icmp = terminator->as<Branch>()->get_cond()->is<Icmp>()) {
                if (const auto &lhs = icmp->get_lhs(), &rhs = icmp->get_rhs();
                    !lhs->is_constant() && !rhs->is_constant()) {
                    const auto idx1 = id_map[lhs], idx2 = id_map[rhs];
                    const auto icmp_op = icmp->icmp_op();

                    if (cfg_graph.predecessors.at(child).size() != 1)
                        continue;

                    if (child == branch->get_true_block()) {
                        child_constraint.add_relation(idx1, idx2, icmp_op);
                    } else if (child == branch->get_false_block()) {
                        child_constraint.add_relation(idx1, idx2, Icmp::inverse_op(icmp_op));
                    }
                }
            }
        }
        run_on_block(child, child_constraint);
    }
}

bool BranchConstrainReduceImpl::impl() {
    Constraint constraint{};
    try {
        run_on_block(current_func->get_blocks().front(), constraint);
    } catch (const std::exception &) {}
    return changed;
}
}

namespace Pass {
void ConstrainReduce::transform(const std::shared_ptr<Module> module) {
    create<StandardizeBinary>()->run_on(module);
    const auto cfg_info = get_analysis_result<ControlFlowGraph>(module);
    const auto dom_info = get_analysis_result<DominanceGraph>(module);
    const auto interval = get_analysis_result<IntervalAnalysis>(module);
    for (const auto &func: module->get_functions()) {
        // ReSharper disable once CppTooWideScopeInitStatement
        BranchConstrainReduceImpl impl{func, cfg_info->graph(func), dom_info->graph(func), interval};
        if (impl.impl()) {
            set_analysis_result_dirty<ControlFlowGraph>(func);
            set_analysis_result_dirty<DominanceGraph>(func);
        }
    }
}
} // namespace Pass
