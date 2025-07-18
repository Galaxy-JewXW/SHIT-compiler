#include "Pass/Transforms/Array.h"
#include "Pass/Transforms/DataFlow.h"
#include "Pass/Util.h"

using namespace Mir;

namespace Pass {
bool SROA::can_be_split(const std::shared_ptr<Alloc> &alloc) {
    if (!alloc->get_type()->as<Type::Pointer>()->get_contain_type()->is_array()) {
        return false;
    }
    std::unordered_set<std::shared_ptr<Instruction>> current_deleted;
    std::vector<std::shared_ptr<Instruction>> alloc_users;

    for (const auto &user: alloc->users()) {
        alloc_users.push_back(user->as<Instruction>());
    }

    for (size_t i = 0; i < alloc_users.size(); ++i) {
        const auto instruction = alloc_users[i];
        if (const auto op = instruction->get_op(); op == Operator::GEP) {
            const auto gep = instruction->as<GetElementPtr>();
            if (!gep->get_index()->is_constant()) {
                return false;
            }
            for (const auto &user: gep->users()) {
                alloc_users.push_back(user->as<Instruction>());
            }
            if (const auto &contain = gep->get_type()->as<Type::Pointer>()->get_contain_type();
                contain->is_integer() || contain->is_float()) {
                int index = **gep->get_index()->as<ConstInt>();
                index_use.try_emplace(index, std::vector<std::shared_ptr<GetElementPtr>>{});
                index_use[index].push_back(gep);
            }
        } else if (op == Operator::BITCAST) {
            for (const auto &user: instruction->users()) {
                alloc_users.push_back(user->as<Instruction>());
            }
            current_deleted.insert(instruction);
        } else if (op == Operator::CALL) {
            if (const auto &func_name = instruction->as<Call>()->get_function()->get_name();
                func_name.find("llvm.memset") == std::string::npos) {
                return false;
            }
            current_deleted.insert(instruction);
        }
    }

    deleted_instructions.insert(current_deleted.begin(), current_deleted.end());
    return true;
}

void SROA::run_on_func(const std::shared_ptr<Function> &func) {
    clear();
    for (const auto &block: func->get_blocks()) {
        for (const auto &instruction: block->get_instructions()) {
            if (instruction->get_op() != Operator::ALLOC) [[likely]] {
                continue;
            }
            index_use = IndexMap{};
            if (const auto alloca = instruction->as<Alloc>(); can_be_split(alloca)) {
                alloc_index_geps[alloca] = index_use;
                deleted_instructions.insert(alloca);
            }
        }
    }
    for (const auto &[alloc, index_geps]: alloc_index_geps) {
        const auto block = alloc->get_block();
        for (const auto &[index, geps]: index_geps) {
            const auto atomic_type =
                    alloc->get_type()->as<Type::Pointer>()->get_contain_type()->as<Type::Array>()->get_atomic_type();
            const auto new_alloc = Alloc::create("alloc", atomic_type, block);
            Utils::move_instruction_before(new_alloc, alloc);
            for (const auto &gep: geps) {
                gep->replace_by_new_value(new_alloc);
                deleted_instructions.insert(gep);
            }
        }
    }
    Utils::delete_instruction_set(Module::instance(), deleted_instructions);
}

void SROA::transform(const std::shared_ptr<Module> module) {
    for (const auto &func: *module) {
        run_on_func(func);
    }
    module->update_id();
    create<Mem2Reg>()->run_on(module);
}

void SROA::transform(const std::shared_ptr<Function> &func) {
    run_on_func(func);
    func->update_id();
    create<Mem2Reg>()->run_on(func);
}

} // namespace Pass
