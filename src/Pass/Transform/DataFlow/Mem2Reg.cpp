#include <deque>
#include <unordered_set>

#include "Mir/Builder.h"
#include "Pass/Analysis.h"
#include "Pass/Transforms/DataFlow.h"
#include "Pass/Util.h"

using namespace Mir;

namespace {
std::string make_name(const std::string &prefix) {
    static int id{0};
    return prefix + std::to_string(++id);
}
}

namespace Pass {
void Mem2Reg::init_mem2reg() {
    use_instructions.clear();
    def_blocks.clear();
    def_instructions.clear();
    def_stack.clear();

    for (const auto &user: current_alloc->users()) {
        auto inst = std::dynamic_pointer_cast<Instruction>(user);
        if (inst == nullptr) {
            log_error("User of %s is not instruction: %s", current_alloc->to_string().c_str(),
                      user->to_string().c_str());
        }
        if (const auto load = std::dynamic_pointer_cast<Load>(inst); load && !load->get_block()->is_deleted()) {
            use_instructions.insert(load);
        }
        if (const auto store = std::dynamic_pointer_cast<Store>(inst); store && !store->get_block()->is_deleted()) {
            def_instructions.insert(store);
            def_blocks.insert(store->get_block());
        }
    }

    // std::ostringstream oss;
    // oss << "Def Insts:\n";
    // for (const auto &inst: def_instructions) {
    //     oss << "  " << inst->to_string() << "\n";
    // }
    // oss << "Def Blocks:\n";
    // for (const auto &block: def_blocks) {
    //     oss << "  " << block->get_name() << "\n";
    // }
    // oss << "Use Insts:\n";
    // for (const auto &inst: use_instructions) {
    //     oss << "  " << inst->to_string() << "\n";
    // }
    // log_debug("\n%s", oss.str().c_str());
}

void Mem2Reg::insert_phi() {
    std::unordered_set<std::shared_ptr<Block>> processed_blocks; // 已处理的基本块
    std::deque worklist(def_blocks.begin(), def_blocks.end());

    while (!worklist.empty()) {
        const auto x = worklist.front();
        worklist.pop_front();

        for (const auto &y: dom_info->graph(current_function).dominance_frontier.at(x)) {
            if (processed_blocks.find(y) == processed_blocks.end()) {
                processed_blocks.insert(y);
                if (def_blocks.find(y) == def_blocks.end()) {
                    worklist.push_back(y);
                }
            }
        }
    }

    for (const auto &b : processed_blocks) {
        std::unordered_map<std::shared_ptr<Block>, std::shared_ptr<Value>> optional_map;
        const auto contain_type =
                std::static_pointer_cast<Type::Pointer>(current_alloc->get_type())->get_contain_type();

        for (const auto &prev_block: cfg_info->graph(current_function).predecessors.at(b)) {
            optional_map[prev_block] = std::make_shared<Value>(make_name("%phi_value_"), contain_type);
        }

        const auto phi = Phi::create(make_name("%phi_"), contain_type, nullptr, optional_map);
        phi->set_block(b, false);
        b->get_instructions().insert(b->get_instructions().begin(), phi);

        use_instructions.insert(phi);
        def_instructions.insert(phi);
    }
}

void Mem2Reg::rename_variables(const std::shared_ptr<Block> &block) {
    int stack_depth{0};
    const auto contain_type = std::static_pointer_cast<Type::Pointer>(current_alloc->get_type())->get_contain_type();

    for (const auto &inst: block->get_instructions()) {
        if (use_instructions.count(inst) && inst->get_op() == Operator::LOAD) {
            const auto load = std::static_pointer_cast<Load>(inst);
            std::shared_ptr<Value> new_value;
            if (!def_stack.empty()) {
                new_value = def_stack.back();
            } else if (contain_type->is_int32()) {
                new_value = ConstInt::create(0);
            } else if (contain_type->is_float()) {
                new_value = ConstFloat::create(0.0f);
            } else {
                log_error("Unsupported type: %s", contain_type->to_string().c_str());
            }
            load->replace_by_new_value(new_value);
        }
        if (def_instructions.count(inst)) {
            if (inst->get_op() == Operator::STORE) {
                def_stack.push_back(inst->as<Store>()->get_value());
            } else if (inst->get_op() == Operator::PHI) {
                def_stack.push_back(inst->as<Phi>());
            } else {
                log_error("Invalid inst: %s", inst->to_string().c_str());
            }
            ++stack_depth;
        }
    }

    // 第二遍遍历：更新后继块的Phi操作数
    for (const auto &succ_block: cfg_info->graph(current_function).successors.at(block)) {
        for (const auto &inst: succ_block->get_instructions()) {
            if (inst->get_op() != Operator::PHI)
                break;

            const auto phi = std::static_pointer_cast<Phi>(inst);
            if (use_instructions.count(inst)) {
                std::shared_ptr<Value> new_value;
                if (!def_stack.empty()) {
                    new_value = def_stack.back();
                } else if (contain_type->is_int32()) {
                    new_value = ConstInt::create(0);
                } else if (contain_type->is_float()) {
                    new_value = ConstFloat::create(0.0);
                } else {
                    log_error("Unsupported type: %s", contain_type->to_string().c_str());
                }
                phi->modify_operand(phi->get_optional_values().at(block), new_value);
            }
        }
    }
    // 递归处理支配子树
    for (const auto &imm_dom_block: dom_info->graph(current_function).dominance_children.at(block)) {
        rename_variables(imm_dom_block);
    }
    // 栈回退操作
    while (stack_depth-- > 0) {
        def_stack.pop_back();
    }
}

void Mem2Reg::run_on_func(const std::shared_ptr<Function> &func) {
    std::vector<std::shared_ptr<Alloc>> valid_allocs;
    for (const auto &block: func->get_blocks()) {
        for (const auto &inst: block->get_instructions()) {
            if (inst->get_op() == Operator::ALLOC) {
                auto alloc = std::static_pointer_cast<Alloc>(inst);
                if (const auto contain_type =
                            std::static_pointer_cast<Type::Pointer>(alloc->get_type())->get_contain_type();
                    !contain_type->is_array()) {
                    valid_allocs.push_back(alloc);
                }
            }
        }
    }
    // 对每个符合条件的Alloc进行Mem2Reg转换
    for (const auto &alloc: valid_allocs) {
        current_alloc = alloc;
        current_function = func;
        init_mem2reg();
        insert_phi();
        rename_variables(current_function->get_blocks().front());
        deleted_instructions.insert(alloc);
        for (const auto &i : def_instructions) {
            if (i->get_op() != Operator::PHI)
                deleted_instructions.insert(i);
        }
        for (const auto &i : use_instructions) {
            if (i->get_op() != Operator::PHI)
                deleted_instructions.insert(i);
        }
        Utils::delete_instruction_set(Module::instance(), deleted_instructions);
        deleted_instructions.clear();
    }
}

void Mem2Reg::transform(const std::shared_ptr<Module> module) {
    set_analysis_result_dirty<ControlFlowGraph>(module);
    cfg_info = get_analysis_result<ControlFlowGraph>(module);
    dom_info = get_analysis_result<DominanceGraph>(module);
    for (const auto &func: *module) {
        run_on_func(func);
    }
    current_alloc = nullptr;
    current_function = nullptr;
    cfg_info = nullptr;
    dom_info = nullptr;
    def_instructions.clear();
    use_instructions.clear();
    def_blocks.clear();
    def_stack.clear();
}
} // namespace Pass
