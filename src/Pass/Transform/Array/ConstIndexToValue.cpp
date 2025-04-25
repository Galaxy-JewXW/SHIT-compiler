#include <deque>

#include "Mir/Init.h"
#include "Pass/Util.h"
#include "Pass/Transforms/Array.h"

using namespace Mir;

namespace {
std::vector<int> gen_indexes(const std::shared_ptr<Type::Array> &type, const int size) {
    int x = size;
    if (static_cast<size_t>(size) > type->get_flattened_size()) {
        log_error("Index out of bound");
    }
    std::shared_ptr<Type::Type> current_type = type;
    std::vector<int> indexes, dimensions;
    while (current_type->is_array()) {
        dimensions.push_back(current_type->as<Type::Array>()->get_size());
        current_type = current_type->as<Type::Array>()->get_element_type();
    }
    for (size_t i = 0; i < dimensions.size(); ++i) {
        int stride = 1;
        for (size_t j = i + 1; j < dimensions.size(); ++j) {
            stride *= dimensions[j];
        }
        int index = x / stride % dimensions[i];
        indexes.push_back(index);
        x -= index * stride;
    }
    return indexes;
}

void transform_global_variable(const std::shared_ptr<GlobalVariable> &gv) {
    if (!gv->get_type()->as<Type::Pointer>()->get_contain_type()->is_array()) {
        return;
    }
    const auto array_type = gv->get_type()->as<Type::Pointer>()->get_contain_type()->as<Type::Array>();
    std::vector<std::shared_ptr<Instruction>> load_instructions{};
    std::deque<std::shared_ptr<Instruction>> use_instructions{};
    std::unordered_set<std::shared_ptr<Instruction>> deleted_instructions{};
    for (const auto &user: gv->users()) {
        use_instructions.push_back(user->as<Instruction>());
    }
    while (!use_instructions.empty()) {
        const auto instruction = use_instructions.front();
        use_instructions.pop_front();
        if (const auto op = instruction->get_op(); op == Operator::STORE || op == Operator::CALL) {
            return;
        } else if (op == Operator::LOAD) {
            load_instructions.push_back(instruction);
        } else if (op == Operator::BITCAST || op == Operator::GEP) {
            for (const auto &user: instruction->users()) {
                use_instructions.push_back(user->as<Instruction>());
            }
        }
    }
    for (const auto &inst: load_instructions) {
        const auto load = inst->as<Load>();
        const auto gep = load->get_addr()->as<GetElementPtr>();

        const auto init_value = gv->get_init_value()->as<Init::Array>();
        if (!gep->get_index()->is_constant() && !init_value->zero_initialized()) {
            continue;
        }
        const auto indexes = gen_indexes(array_type, **gep->get_index()->as<ConstInt>());
        const auto constant_value = init_value->get_init_value(indexes)->as<Init::Constant>();
        const auto value = constant_value->get_const_value();
        load->replace_by_new_value(value);
        deleted_instructions.insert(load);
    }
    Pass::Utils::delete_instruction_set(Module::instance(), deleted_instructions);
}
}

void Pass::ConstIndexToValue::transform(const std::shared_ptr<Module> module) {
    std::for_each(module->get_global_variables().begin(), module->get_global_variables().end(),
                  transform_global_variable);
}
