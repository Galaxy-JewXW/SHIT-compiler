#include <queue>

#include "Pass/Util.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
class InBlockScheduler {
    struct SchedulerInstruction {
    private:
        static int cnt;

    public:
        std::shared_ptr<Instruction> instruction;
        int score;
        int timestamp;

        static void reset() { cnt = 0; }

        SchedulerInstruction(const std::shared_ptr<Instruction> &instruction, const int score) :
            instruction{instruction}, score{score}, timestamp{++cnt} {}

        bool operator<(const SchedulerInstruction &other) const {
            return score != other.score ? score > other.score : timestamp < other.timestamp;
        }
    };

    const std::shared_ptr<Block> &block;
    std::vector<std::shared_ptr<Instruction>> &instructions;
    const std::shared_ptr<Pass::FunctionAnalysis> &func_info;
    std::unordered_map<std::shared_ptr<Block>, std::unordered_set<std::shared_ptr<Instruction>>> &out_var_map;
    // 每个 block 的活跃指令对应的 use -> user
    std::unordered_map<std::shared_ptr<Instruction>, std::unordered_set<std::shared_ptr<Instruction>>> live_map{};
    // 每个指令的分数
    std::unordered_map<std::shared_ptr<Instruction>, int> score_map{};
    // 位于队列中的指令
    std::unordered_set<std::shared_ptr<Instruction>> pending_instructions{};
    // user -> use
    std::unordered_map<std::shared_ptr<Instruction>, std::unordered_set<std::shared_ptr<Instruction>>> use_map{};
    // use -> user
    std::unordered_map<std::shared_ptr<Instruction>, std::unordered_set<std::shared_ptr<Instruction>>> user_map{};

    std::priority_queue<SchedulerInstruction> schedule_queue{};

    void init();

    SchedulerInstruction make_scheduler_instruction(const std::shared_ptr<Instruction> &instruction, int score);

    [[nodiscard]] bool is_pinned(const std::shared_ptr<Instruction> &instruction) const;

    void update_score(const std::shared_ptr<Instruction> &instruction, int score);

    std::shared_ptr<Instruction> get_from_queue();

public:
    explicit InBlockScheduler(const std::shared_ptr<Block> &block,
                              const std::shared_ptr<Pass::FunctionAnalysis> &func_info,
                              std::unordered_map<std::shared_ptr<Block>, std::unordered_set<std::shared_ptr<
                                                     Instruction>>> &out_var_map) :
        block{block}, instructions{block->get_instructions()}, func_info{func_info}, out_var_map{out_var_map} {
        SchedulerInstruction::reset();
    }

    void schedule();
};

int InBlockScheduler::SchedulerInstruction::cnt = 0;

bool InBlockScheduler::is_pinned(const std::shared_ptr<Instruction> &instruction) const {
    switch (instruction->get_op()) {
        case Operator::LOAD:
        case Operator::STORE:
            return true;
        case Operator::CALL: {
            const auto called_func{instruction->as<Call>()->get_function()->as<Function>()};
            if (called_func->is_runtime_func()) {
                if (const auto name = called_func->get_name();
                    name.find("get") != std::string::npos || name.find("put") != std::string::npos) {
                    return true;
                }
                return false;
            }
            if (!func_info->func_info(called_func).no_state)
                return true;
        }
        default:
            break;
    }
    return false;
}

InBlockScheduler::SchedulerInstruction InBlockScheduler::make_scheduler_instruction(
        const std::shared_ptr<Instruction> &instruction,
        const int score) {
    const auto scheduler_instruction = SchedulerInstruction{instruction, score};
    pending_instructions.insert(scheduler_instruction.instruction);
    return scheduler_instruction;
}

void InBlockScheduler::update_score(const std::shared_ptr<Instruction> &instruction, const int score) {
    score_map[instruction] += score;
    if (pending_instructions.count(instruction)) {
        schedule_queue.push(make_scheduler_instruction(instruction, score));
    }
}

void InBlockScheduler::init() {
    const auto l{instructions.size() - 1};

    std::unordered_set<std::shared_ptr<Instruction>> pass_set;
    std::shared_ptr<Instruction> last_pinned{nullptr};

#define ADD_USE(user, used) do { use_map[user].insert(used); user_map[used].insert(user); } while (0)

    for (size_t i = 0; i < l; ++i) {
        const auto &inst{instructions[i]};
        if (inst->get_op() == Operator::PHI)
            continue;
        for (const auto &operand: inst->get_operands()) {
            if (const auto op_inst{operand->is<Instruction>()};
                op_inst != nullptr && pass_set.count(op_inst)) {
                ADD_USE(inst, op_inst);
            }
        }
        if (inst->get_op() == Operator::CALL) {
            const auto call{inst->as<Call>()};
            if (const auto name{call->get_function()->get_name()};
                name == "starttime" || name == "stoptime") {
                for (const auto &_inst: pass_set) {
                    ADD_USE(call, _inst);
                }
            }
            if (const auto next_inst{*std::next(Pass::Utils::inst_as_iter(call).value())};
                next_inst->is<Terminator>() == nullptr) {
                ADD_USE(next_inst, call);
            }
        }
        if (is_pinned(inst)) {
            if (last_pinned != nullptr) {
                ADD_USE(inst, last_pinned);
            }
            last_pinned = inst;
        }
        pass_set.insert(inst);
    }

#undef ADD_USE

    const auto &exit{out_var_map.at(block)};
    for (size_t i = 0; i < l; ++i) {
        const auto &inst{instructions[i]};
        if (inst->get_op() == Operator::PHI)
            continue;
        for (const auto &operand: inst->get_operands()) {
            if (const auto op_inst{operand->is<Instruction>()};
                op_inst != nullptr && !exit.count(op_inst)) {
                live_map[op_inst].insert(inst);
            }
        }
    }
}

std::shared_ptr<Instruction> InBlockScheduler::get_from_queue() {
    while (!schedule_queue.empty()) {
        const auto inst{schedule_queue.top()};
        schedule_queue.pop();
        // 处理过时的指令项
        if (inst.score != score_map[inst.instruction])
            continue;
        // 确保不会重复调度同一个指令
        if (!pending_instructions.count(inst.instruction))
            continue;
        pending_instructions.erase(inst.instruction);
        return inst.instruction;
    }
    return nullptr;
}

void InBlockScheduler::schedule() {
    log_trace("%s", block->get_name().c_str());
    const auto snap{instructions};
    const auto terminator{instructions.back()};
    init();

    std::vector<std::shared_ptr<Instruction>> phi_instructions;
    for (const auto &inst: instructions) {
        if (inst->get_op() == Operator::PHI) {
            phi_instructions.push_back(inst);
        } else {
            break;
        }
    }

    const auto l{instructions.size() - 1};
    for (size_t i = 0; i < l; ++i) {
        const auto &inst{instructions[i]};
        if (inst->get_op() == Operator::PHI)
            continue;
        score_map[inst] = 0;
        if (user_map[inst].empty()) {
            schedule_queue.push(make_scheduler_instruction(inst, 0));
        }
        if (*inst->get_type() != *Type::Void::void_) {
            update_score(inst, -1);
        }
    }

    instructions.clear();
    instructions.insert(instructions.end(), phi_instructions.begin(), phi_instructions.end());
    instructions.push_back(terminator);

    std::for_each(live_map.begin(), live_map.end(), [&](const auto &pair) {
        update_score(*pair.second.begin(), 1);
        out_var_map[block].insert(pair.first);
    });
    // (Uses(B) ∪ LiveOut(B)) - Defs(B)
    std::for_each(snap.begin(), snap.end(), [&](const auto &inst) { out_var_map[block].erase(inst); });

    std::shared_ptr<Instruction> pos = terminator;
    while (true) {
        const auto inst = get_from_queue();
        if (inst == nullptr)
            break;
        instructions.push_back(inst);
        Pass::Utils::move_instruction_before(inst, pos);
        pos = inst;

        for (const auto &operand: inst->get_operands()) {
            if (const auto op_inst{operand->is<Instruction>()};
                op_inst && live_map.count(op_inst)) {
                auto &users{live_map[op_inst]};
                users.erase(inst);
                for (const auto &user: users) {
                    update_score(user, -1);
                }
            }
        }

        for (const auto &use: use_map[inst]) {
            auto &users{user_map[use]};
            users.erase(inst);
            if (users.empty()) {
                schedule_queue.push(make_scheduler_instruction(use, score_map[use]));
                pending_instructions.insert(use);
            }
        }
    }

    if (instructions.size() != snap.size()) [[unlikely]] {
        // something went wrong. just revert.
        instructions = snap;
    }
}
} // namespace

namespace Pass {
void InstSchedule::in_block_schedule(const std::shared_ptr<Function> &func) const {
    std::unordered_map<std::shared_ptr<Block>, std::unordered_set<std::shared_ptr<Instruction>>> out_live_variables;
    for (const auto &block: func->get_blocks()) {
        out_live_variables[block] = {};
    }
    for (const auto &block: func->get_blocks()) {
        for (const auto &inst: block->get_instructions()) {
            if (inst->get_op() != Operator::PHI)
                break;
            const auto phi{inst->as<Phi>()};
            for (const auto &[b, v]: phi->get_optional_values()) {
                if (const auto i{v->is<Instruction>()})
                    out_live_variables[b].insert(i);
            }
        }
    }

    const auto &graph{dom_graph->graph(func)};
    auto dfs = [&](auto &&self, const std::shared_ptr<Block> &block) -> void {
        const auto &terminator{block->get_instructions().back()};
        for (const auto &operand: terminator->get_operands()) {
            if (const auto i{operand->is<Instruction>()})
                out_live_variables[block].insert(i);
        }
        for (const auto &child: graph.dominance_children.at(block)) {
            self(self, child);
            out_live_variables[block].insert(out_live_variables[child].begin(), out_live_variables[child].end());
        }
        InBlockScheduler scheduler{block, func_info, out_live_variables};
        scheduler.schedule();
    };
    dfs(dfs, func->get_blocks().front());
}

void InstSchedule::run_on_func(const std::shared_ptr<Function> &func) const {
    in_block_schedule(func);
}

void InstSchedule::transform(const std::shared_ptr<Module> module) {
    dom_graph = get_analysis_result<DominanceGraph>(module);
    func_info = get_analysis_result<FunctionAnalysis>(module);
    for (const auto &func: module->get_functions()) {
        run_on_func(func);
    }
    dom_graph = nullptr;
    func_info = nullptr;
}
} // namespace Pass
