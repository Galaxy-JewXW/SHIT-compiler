#include "Pass/Util.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
struct Helper {
private:
    const std::shared_ptr<Block> &block;
    const Pass::ControlFlowGraph::Graph &cfg_info;
    const std::vector<std::shared_ptr<Phi>> &phis;
    std::unordered_map<std::shared_ptr<Phi>, std::shared_ptr<Value>> phi_map;

    [[nodiscard]] static std::string make_name();

    [[nodiscard]] bool is_critical_edge(const std::shared_ptr<Block> &prev, const std::shared_ptr<Block> &succ) const;

    [[nodiscard]] std::shared_ptr<Block> split_critical_edge(const std::shared_ptr<Block> &prev,
                                                             const std::shared_ptr<Block> &succ) const;

    void insert_moves(const std::shared_ptr<Block> &prev, const std::vector<std::shared_ptr<Move>> &moves) const;

public:
    Helper(const std::shared_ptr<Block> &block, const Pass::ControlFlowGraph::Graph &cfg_info,
           const std::vector<std::shared_ptr<Phi>> &phis) :
        block{block}, cfg_info{cfg_info}, phis{phis} {}

    void build();
};

std::string Helper::make_name() {
    static int id{0};
    return "%temp_" + std::to_string(++id);
}

bool Helper::is_critical_edge(const std::shared_ptr<Block> &prev, const std::shared_ptr<Block> &succ) const {
    return cfg_info.predecessors.at(succ).size() > 1 && cfg_info.successors.at(prev).size() > 1;
}

std::shared_ptr<Block> Helper::split_critical_edge(const std::shared_ptr<Block> &prev, decltype(prev) succ) const {
    const auto split_block = Block::create("split_block", block->get_function());
    Jump::create(succ, split_block);
    prev->modify_successor(succ, split_block);
    for (const auto &inst: succ->get_instructions()) {
        if (inst->get_op() != Operator::PHI)
            break;
        if (const auto phi{inst->as<Phi>()}; phi->get_optional_values().count(prev)) {
            phi->modify_operand(prev, split_block);
        }
    }
    return split_block;
}

void Helper::insert_moves(const std::shared_ptr<Block> &prev, const std::vector<std::shared_ptr<Move>> &moves) const {
    // 检查 prev -> block 是否为关键边，决定插入点
    const std::shared_ptr<Block> insertion_block = is_critical_edge(prev, block)
                                                       ? split_critical_edge(prev, block)
                                                       : prev;

    // 解决并行拷贝
    std::unordered_set pending_moves(moves.begin(), moves.end());
    std::vector<std::shared_ptr<Move>> ordered_moves;
    ordered_moves.reserve(pending_moves.size());

    while (!pending_moves.empty()) {
        // 寻找可以安全执行的move（拓扑排序步骤）
        // 一个move(dest, src)是安全的，如果它的源(src)不被任何其他待处理的move所覆盖（即src不是任何其他pendingMove的目标）
        decltype(ordered_moves) ready_moves;
        std::unordered_set<std::shared_ptr<Value>> pending_destinations;
        for (const auto &move: pending_moves) {
            pending_destinations.insert(move->get_to_value());
        }
        for (const auto &move: pending_moves) {
            if (pending_destinations.find(move->get_from_value()) == pending_destinations.end()) {
                ready_moves.push_back(move);
            }
        }

        // 如果找到了一个或多个安全的move，将它们添加到结果中并从未决列表中移除
        if (!ready_moves.empty()) {
            for (const auto &move: ready_moves) {
                ordered_moves.push_back(move);
                pending_moves.erase(move);
            }
            continue;
        }

        // 如果没找到安全的move，说明存在循环依赖
        // 从循环中任意选择一个move来打破它，例如 p = (dest, src)
        const auto p{*pending_moves.begin()};
        const auto dest{p->get_to_value()}, src{p->get_from_value()};
        const auto temp{std::make_shared<Value>(make_name(), dest->get_type())};
        // 保存目标(dest)的原始值到临时变量中
        // 这是破环的关键：在dest被覆盖前保存它的值
        ordered_moves.push_back(Move::create(temp, dest, nullptr));
        // 用(dest, src)这条move来更新待处理列表
        // 我们已经将dest的原始值保存到了temp中，现在可以安全地覆盖dest了。
        // 我们将 (dest, src) 这条指令本身添加到结果列表中。
        ordered_moves.push_back(Move::create(dest, src, nullptr));
        pending_moves.erase(p);
        // 更新依赖关系：
        // 将所有依赖于dest原始值的move，改为依赖于temp
        for (const auto &other_move: pending_moves) {
            if (other_move->get_from_value() == dest) {
                other_move->modify_operand(other_move->get_from_value(), temp);
            }
        }
    }

    for (const auto &move: ordered_moves) {
        const auto terminator{insertion_block->get_instructions().back()};
        move->set_block(insertion_block);
        Pass::Utils::move_instruction_before(move, terminator);
    }
}

void Helper::build() {
    std::unordered_map<std::shared_ptr<Block>, std::vector<std::shared_ptr<Move>>> move_map;

    // 收集所有 move 操作，并按照前驱块分组
    for (const auto &phi: phis) {
        phi_map[phi] = std::make_shared<Value>(make_name(), phi->get_type());
        for (const auto &[pre, value]: phi->get_optional_values()) {
            move_map.try_emplace(pre, std::vector<std::shared_ptr<Move>>{});
            move_map[pre].push_back(Move::create(phi_map[phi], value, nullptr));
        }
        phi->replace_by_new_value(phi_map[phi]);
    }

    // 为需要插入 move 的前驱块进行插入操作
    for (const auto &[pre, moves]: move_map) {
        insert_moves(pre, moves);
    }
}
}

namespace Pass {
void RemovePhi::run_on_func(const std::shared_ptr<Function> &func) {
    for (const auto &block: func->get_blocks()) {
        if (block->get_instructions().front()->get_op() != Operator::PHI)
            continue;
        std::vector<std::shared_ptr<Phi>> phis;
        for (const auto &inst: block->get_instructions()) {
            if (inst->get_op() != Operator::PHI)
                break;
            phis.push_back(inst->as<Phi>());
        }
        Helper helper{block, cfg_info->graph(func), phis};
        helper.build();
        to_be_deleted.insert(phis.begin(), phis.end());
        set_analysis_result_dirty<ControlFlowGraph>(func);
    }
}

void RemovePhi::transform(const std::shared_ptr<Module> module) {
    cfg_info = get_analysis_result<ControlFlowGraph>(module);
    for (const auto &func: module->get_functions()) {
        run_on_func(func);
    }
    Utils::delete_instruction_set(module, to_be_deleted);
    to_be_deleted.clear();
    cfg_info = nullptr;
}
}
