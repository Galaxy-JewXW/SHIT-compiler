#include <unordered_set>

#include "Mir/Builder.h"
#include "Pass/Analysis.h"
#include "Pass/Transform.h"

using namespace Mir;

namespace Pass {
void Mem2Reg::init_mem2reg() {
    use_instructions.clear();
    def_blocks.clear();
    def_instructions.clear();
    def_stack.clear();

    for (const auto &user: current_alloc->users()) {
        auto inst = std::dynamic_pointer_cast<Instruction>(user);
        if (inst == nullptr) {
            log_error("User of %s is not instruction: %s",
                      current_alloc->to_string().c_str(), user->to_string().c_str());
        }
        if (const auto load = std::dynamic_pointer_cast<Load>(inst);
            load && !load->get_block()->is_deleted()) {
            use_instructions.emplace_back(load);
        }
        if (const auto store = std::dynamic_pointer_cast<Store>(inst);
            store && !store->get_block()->is_deleted()) {
            def_instructions.emplace_back(store);
            if (std::find(def_blocks.begin(), def_blocks.end(), store->get_block()) == def_blocks.end()) {
                def_blocks.emplace_back(store->get_block());
            }
        }
    }
}

void Mem2Reg::insert_phi() {
    std::unordered_set<std::shared_ptr<Block>> processed_blocks; // 已处理的基本块
    std::vector<std::shared_ptr<Block>> worklist = def_blocks;
    while (!worklist.empty()) {
        const auto x = worklist.front();
        worklist.erase(worklist.begin());
        for (const auto &y: cfg_info->dominance_frontier(current_function).at(x)) {
            if (processed_blocks.find(y) != processed_blocks.end()) continue;
            std::unordered_map<std::shared_ptr<Block>, std::shared_ptr<Value>> optional_map;
            for (const auto &prev_block: cfg_info->predecessors(current_function).at(y)) {
                optional_map[prev_block] = nullptr;
            }
            const auto contain_type = std::static_pointer_cast<Type::Pointer>(current_alloc->get_type())
                  ->get_contain_type();
            const auto phi = Phi::create(Builder::gen_variable_name(), contain_type,
                                         nullptr, optional_map);
            y->get_instructions().insert(y->get_instructions().begin(), phi);
            use_instructions.emplace_back(phi);
            def_instructions.emplace_back(phi);
            processed_blocks.insert(y);
            if (std::find(def_blocks.begin(), def_blocks.end(), y) == def_blocks.end()) {
                worklist.emplace_back(y);
            }
        }
    }
}

void Mem2Reg::rename_variables(const std::shared_ptr<Block> &block) {
    int stack_depth{0};
    const auto contain_type = std::static_pointer_cast<Type::Pointer>(current_alloc->get_type())
          ->get_contain_type();
    for (auto it = block->get_instructions().begin(); it != block->get_instructions().end();) {
        if (auto instruction = *it; instruction == current_alloc) {
            it = block->get_instructions().erase(it);
        } else if (const auto load = std::dynamic_pointer_cast<Load>(instruction);
            load && std::find(use_instructions.begin(), use_instructions.end(), load) != use_instructions.end()) {
            std::shared_ptr<Value> new_value;
            if (!def_stack.empty()) {
                new_value = def_stack.back();
            } else if (contain_type->is_int32()) {
                new_value = std::make_shared<ConstInt>(0);
            } else if (contain_type->is_float()) {
                new_value = std::make_shared<ConstFloat>(0.0f);
            } else {
                log_error("Unsupported type: %s", contain_type->to_string().c_str());
            }
            load->replace_by_new_value(new_value);
            it = block->get_instructions().erase(it);
        } else if (const auto store = std::dynamic_pointer_cast<Store>(instruction);
            store && std::find(def_instructions.begin(), def_instructions.end(), store) != def_instructions.end()) {
            const auto stored_value = store->get_value();
            def_stack.emplace_back(stored_value);
            store->clear_operands();
            ++stack_depth;
            it = block->get_instructions().erase(it);
        } else if (const auto phi = std::dynamic_pointer_cast<Phi>(instruction);
            phi && std::find(def_instructions.begin(), def_instructions.end(), phi) != def_instructions.end()) {
            def_stack.emplace_back(phi);
            ++stack_depth;
            ++it;
        } else {
            ++it;
        }
    }
    // 第二遍遍历：更新后继块的Phi操作数
    for (const auto &succ_block: cfg_info->successors(current_function).at(block)) {
        const auto first_instruction = succ_block->get_instructions().front();
        if (const auto phi = std::dynamic_pointer_cast<Phi>(first_instruction);
            phi && std::find(use_instructions.begin(), use_instructions.end(), phi) != use_instructions.end()) {
            std::shared_ptr<Value> new_value;
            if (!def_stack.empty()) {
                new_value = def_stack.back();
            } else if (contain_type->is_int32()) {
                new_value = std::make_shared<ConstInt>(0);
            } else if (contain_type->is_float()) {
                new_value = std::make_shared<ConstFloat>(0.0f);
            } else {
                log_error("Unsupported type: %s", contain_type->to_string().c_str());
            }
            phi->set_optional_value(block, new_value);
        }
    }
    // 递归处理支配子树
    for (const auto &imm_dom_block: cfg_info->dominance_children(current_function).at(block)) {
        rename_variables(imm_dom_block);
    }
    // 栈回退操作
    while (stack_depth-- > 0) {
        def_stack.pop_back();
    }
}

void Mem2Reg::transform(const std::shared_ptr<Module> module) {
    for (const auto &func: *module) {
        // 收集当前函数的所有Alloc指令
        std::vector<std::shared_ptr<Alloc>> valid_allocs;
        for (const auto &block: func->get_blocks()) {
            for (const auto &inst: block->get_instructions()) {
                if (inst->get_op() == Operator::ALLOC) {
                    auto alloc = std::static_pointer_cast<Alloc>(inst);
                    if (const auto contain_type = std::static_pointer_cast<Type::Pointer>(alloc->get_type())->
                            get_contain_type(); !contain_type->is_array()) {
                        valid_allocs.emplace_back(alloc);
                    }
                }
            }
        }
        // 对每个符合条件的Alloc进行Mem2Reg转换
        for (const auto &alloc: valid_allocs) {
            current_alloc = alloc;
            current_function = func;
            def_blocks.clear();
            init_mem2reg();
            insert_phi();
            rename_variables(current_function->get_blocks().front());
        }
    }
    current_alloc = nullptr;
    current_function = nullptr;
    def_instructions.clear();
    use_instructions.clear();
    def_blocks.clear();
    def_stack.clear();
}
}
