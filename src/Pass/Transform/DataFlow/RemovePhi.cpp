#include "Pass/Transforms/DataFlow.h"
#include "Pass/Util.h"

using namespace Mir;

namespace {
struct Helper {
private:
    const std::shared_ptr<Block> &block;
    const Pass::ControlFlowGraph::Graph &cfg_info;
    const std::vector<std::shared_ptr<Phi>> &phis;
    std::unordered_map<std::shared_ptr<Phi>, std::shared_ptr<Value>> phi_map;

    [[nodiscard]] static std::string make_name(const std::string &prefix);

    [[nodiscard]] bool is_critical_edge(const std::shared_ptr<Block> &prev, const std::shared_ptr<Block> &succ) const;

    [[nodiscard]] std::shared_ptr<Block> split_critical_edge(const std::shared_ptr<Block> &prev,
                                                             const std::shared_ptr<Block> &succ) const;

    void insert_moves(const std::shared_ptr<Block> &prev, const std::vector<std::shared_ptr<Move>> &moves) const;

public:
    Helper(const std::shared_ptr<Block> &block, const Pass::ControlFlowGraph::Graph &cfg_info,
           const std::vector<std::shared_ptr<Phi>> &phis) : block{block}, cfg_info{cfg_info}, phis{phis} {}

    std::unordered_set<std::shared_ptr<Value>> phicopy_variables{};

    void build();
};

std::string Helper::make_name(const std::string &prefix) {
    static int id{0};
    return prefix + std::to_string(++id);
}

bool Helper::is_critical_edge(const std::shared_ptr<Block> &prev, const std::shared_ptr<Block> &succ) const {
    return cfg_info.predecessors.at(succ).size() > 1 && cfg_info.successors.at(prev).size() > 1;
}

std::shared_ptr<Block> Helper::split_critical_edge(const std::shared_ptr<Block> &prev, decltype(prev) succ) const {
    const auto split_block = Block::create(make_name("split_block_"), block->get_function());
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

void Helper::insert_moves(const std::shared_ptr<Block> &prev,
                          const std::vector<std::shared_ptr<Move>> &moves) const {
    const std::shared_ptr<Block> insertion_block =
            is_critical_edge(prev, block) ? split_critical_edge(prev, block) : prev;

    std::vector<std::shared_ptr<Move>> final_moves;
    std::unordered_set<std::shared_ptr<Value>> destinations;
    std::unordered_map<std::shared_ptr<Value>, std::shared_ptr<Value>> saved_values;

    // 收集所有目标
    for (const auto &move : moves) {
        destinations.insert(move->get_to_value());
    }

    // 保存所有需要被覆盖的源值
    // 如果一个 move 的源(src) 也是其他 move 的目标(dest)，
    // 那么这个 src 的值在后续会被覆盖，需要提前保存。
    for (const auto &move : moves) {
        // 如果源也是一个目标，并且我们还没有为它创建临时变量
        if (auto src = move->get_from_value(); destinations.count(src) && !saved_values.count(src)) {
            const auto temp = std::make_shared<Value>(make_name("%temp_"), src->get_type());
            final_moves.push_back(Move::create(temp, src, nullptr)); // temp = src
            saved_values[src] = temp;
        }
    }

    // 使用保存好的值（如果存在）或原始值进行赋值
    for (const auto &move : moves) {
        auto dest = move->get_to_value();
        if (auto src = move->get_from_value(); saved_values.count(src)) {
            // 使用临时变量中的值
            final_moves.push_back(Move::create(dest, saved_values[src], nullptr));
        } else {
            // 源是安全的，直接赋值
            final_moves.push_back(Move::create(dest, src, nullptr));
        }
    }

    // 将排序好的指令插入到块中
    for (const auto &move : final_moves) {
        const auto terminator = insertion_block->get_instructions().back();
        move->set_block(insertion_block);
        Pass::Utils::move_instruction_before(move, terminator);
    }
}


void Helper::build() {
    std::unordered_map<std::shared_ptr<Block>, std::vector<std::shared_ptr<Move>>> move_map;

    // 收集所有 move 操作，并按照前驱块分组
    for (const auto &phi: phis) {
        const auto phicopy_value = std::make_shared<Value>(make_name("%temp_"), phi->get_type());
        phi_map[phi] = phicopy_value;
        // log_debug("%s -> %s", phi->to_string().c_str(), phicopy_value->get_name().c_str());
        phicopy_variables.insert(phicopy_value);
        for (const auto &[pre, value]: phi->get_optional_values()) {
            if (value != phi) [[likely]] {
                move_map.try_emplace(pre, std::vector<std::shared_ptr<Move>>{});
                move_map[pre].push_back(Move::create(phi_map[phi], value, nullptr));
            }
        }
        phi->replace_by_new_value(phi_map[phi]);
    }

    // 为需要插入 move 的前驱块进行插入操作
    for (const auto &[pre, moves]: move_map) {
        insert_moves(pre, moves);
    }
}
} // namespace

namespace Pass {
void RemovePhi::run_on_func(const std::shared_ptr<Function> &func) {
    const auto snap{func->get_blocks()};
    for (const auto &block: snap) {
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
        func->phicopy_values().insert(func->phicopy_values().end(), helper.phicopy_variables.begin(),
                                      helper.phicopy_variables.end());
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
    set_analysis_result_dirty<ControlFlowGraph>(module);
    cfg_info = nullptr;
}
} // namespace Pass
