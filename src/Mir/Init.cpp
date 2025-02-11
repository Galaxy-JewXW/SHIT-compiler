#include "Mir/Init.h"
#include "Mir/Builder.h"
#include "Mir/Instruction.h"
#include "Utils/Log.h"

namespace Mir::Init {
std::shared_ptr<Constant> Constant::create_constant_init_value(const std::shared_ptr<Type::Type> &type,
                                                               const std::shared_ptr<AST::AddExp> &addExp,
                                                               const std::shared_ptr<Symbol::Table> &table) {
    const auto &res = eval_exp(addExp, table);
    if (type->is_int32()) {
        int value = std::visit([](auto &&arg) { return static_cast<int>(arg); }, res);
        return std::make_shared<Constant>(type, std::make_shared<ConstInt>(value));
    }
    if (type->is_float()) {
        float value = std::visit([](auto &&arg) { return static_cast<float>(arg); }, res);
        return std::make_shared<Constant>(type, std::make_shared<ConstFloat>(value));
    }
    log_error("Illegal type: %s", type->to_string().c_str());
}

std::shared_ptr<Constant> Constant::create_zero_constant_init_value(const std::shared_ptr<Type::Type> &type) {
    if (type->is_int32()) {
        return std::make_shared<Constant>(
            type, std::make_shared<ConstInt>(0));
    }
    if (type->is_float()) {
        return std::make_shared<Constant>(
            type, std::make_shared<ConstFloat>(0.0f));
    }
    log_error("Illegal type: %s", type->to_string().c_str());
}

std::shared_ptr<Init> Exp::create_exp_init_value(const std::shared_ptr<Type::Type> &type,
                                                 const std::shared_ptr<Value> &exp_value) {
    if (const auto &const_ = std::dynamic_pointer_cast<Const>(exp_value)) {
        return std::make_shared<Constant>(type, const_);
    }
    return std::make_shared<Exp>(type, exp_value);
}

[[nodiscard]] std::shared_ptr<Init> Array::get_init_value(const std::vector<int> &indexes) {
    if (!type->is_array()) {
        log_error("Illegal type: %s", type->to_string().c_str());
    }
    auto current_type = type;
    size_t dim_count = 0;
    while (current_type->is_array()) {
        const auto arr_type = std::static_pointer_cast<Type::Array>(current_type);
        if (dim_count >= indexes.size()) break;
        if (indexes[dim_count] < 0 || (indexes[dim_count]) >= static_cast<int>(arr_type->get_size())) {
            log_error("Index out of range[%zu]: [0, %zu) vs %d",
                      dim_count, arr_type->get_size(), indexes[dim_count]);
        }
        current_type = arr_type->get_element_type();
        dim_count++;
    }
    if (dim_count != indexes.size()) {
        log_error("Index depth %zu mismatch array dimensions %zu", indexes.size(), dim_count);
    }
    std::shared_ptr<Init> current_init = shared_from_this();
    for (const int &idx: indexes) {
        const auto arr = std::dynamic_pointer_cast<Array>(current_init);
        if (!arr) { log_error("Indexing into non-array init value"); }
        if (arr->is_zero_initialized) {
            return Constant::create_zero_constant_init_value(
                std::static_pointer_cast<Type::Array>(arr->type)->get_element_type());
        }
        if (idx < 0 || idx >= static_cast<int>(arr->init_values.size())) {
            log_error("Index %d out of array bounds[0, %zu)", idx, arr->init_values.size());
        }
        current_init = arr->init_values[idx];
    }
    if (current_init->is_array_init()) {
        log_error("Remaining array type after full indexing");
    }
    return current_init;
}


void Constant::gen_store_inst(const std::shared_ptr<Value> &addr, const std::shared_ptr<Block> &block) {
    if (!addr->get_type()->is_pointer()) { log_error("Illegal type: %s", addr->get_type()->to_string().c_str()); }
    const auto &self = std::static_pointer_cast<Constant>(shared_from_this());
    const auto &value = self->get_const_value();
    const auto &content_type = std::static_pointer_cast<Type::Pointer>(addr->get_type())->get_contain_type();
    const auto &casted_value = type_cast(value, content_type, block);
    Store::create(addr, casted_value, block);
}

void Exp::gen_store_inst(const std::shared_ptr<Value> &addr, const std::shared_ptr<Block> &block) {
    if (!addr->get_type()->is_pointer()) { log_error("Illegal type: %s", addr->get_type()->to_string().c_str()); }
    const auto &self = std::static_pointer_cast<Exp>(shared_from_this());
    const auto &value = self->get_exp_value();
    const auto &content_type = std::static_pointer_cast<Type::Pointer>(addr->get_type())->get_contain_type();
    const auto &casted_value = type_cast(value, content_type, block);
    Store::create(addr, casted_value, block);
}

std::shared_ptr<Array> Array::create_zero_array_init_value(const std::shared_ptr<Type::Type> &type) {
    if (!type->is_array()) {
        log_error("%s is not an array type", type->to_string().c_str());
    }
    return std::make_shared<Array>(type, std::vector<std::shared_ptr<Init>>{}, true);
}

void Array::gen_store_inst(const std::shared_ptr<Value> &addr, const std::shared_ptr<Block> &block,
                           const std::vector<int> &dimensions) const {
    if (!addr->get_type()->is_pointer()) {
        log_error("Illegal type: %s", addr->get_type()->to_string().c_str());
    }
    if (std::dynamic_pointer_cast<Alloc>(addr)) {
        const auto array_type = std::static_pointer_cast<Type::Array>(type);
        const auto atomic_type = array_type->get_atomic_type();
        size_t element_size = 0;
        // 基本类型占4字节
        if (atomic_type->is_int32() || atomic_type->is_float()) {
            element_size = 4;
        } else {
            log_error("Unsupported atomic type for memset initialization");
        }
        int total_elements = 1;
        for (const int &dim: dimensions) {
            if (dim <= 0) {
                log_error("Illegal dimension: %d", dim);
            }
            total_elements *= dim;
        }
        const size_t total_bytes = total_elements * element_size;
        const auto i8_ptr_type = std::make_shared<Type::Pointer>(Type::Integer::i8);
        const auto bitcast = BitCast::create(Builder::gen_variable_name(), addr, i8_ptr_type, block);
        const auto func_memset = Function::runtime_functions.find("llvm.memset.p0i8.i64")->second;
        const auto size_val = std::make_shared<ConstInt>(static_cast<int>(total_bytes), Type::Integer::i64),
                   zero_val = std::make_shared<ConstInt>(0, Type::Integer::i8),
                   is_volatile = std::make_shared<ConstInt>(0, Type::Integer::i1);
        // 用 memset 将整个数组内存区域置 0
        Call::create(func_memset, {bitcast, size_val, zero_val, is_volatile}, block);
    }
    // 遍历 init_values，对于非零初始化的元素生成 store 指令
    for (size_t i = 0; i < init_values.size(); ++i) {
        auto init = init_values[i];
        // 如果当前元素为零初始化，则跳过（memset 已完成清零）
        bool skip = false;
        if (init->is_constant_init()) {
            if (const auto constant_init = std::static_pointer_cast<Constant>(init); constant_init->is_zero()) {
                skip = true;
            }
        } else if (init->is_array_init()) {
            if (const auto array_init = std::static_pointer_cast<Array>(init); array_init->is_zero_initialized) {
                skip = true;
            }
        }
        if (skip) continue;
        auto index_val = std::make_shared<ConstInt>(static_cast<int>(i));
        auto element_addr = GetElementPtr::create(Builder::gen_variable_name(), addr, index_val, block);
        // 如果当前元素是子数组，则递归生成内部 store 指令
        if (init->is_array_init()) {
            // 对于子数组，其对应的维度为原 dimensions 去掉第一维
            if (dimensions.size() < 2) {
                log_error("Insufficient dimensions for array initializer");
            }
            std::vector sub_dimensions(dimensions.begin() + 1, dimensions.end());
            std::static_pointer_cast<Array>(init)->gen_store_inst(element_addr, block, sub_dimensions);
        } else if (init->is_exp_init() || init->is_constant_init()) {
            if (init->is_constant_init()) {
                std::static_pointer_cast<Constant>(init)->gen_store_inst(element_addr, block);
            } else {
                std::static_pointer_cast<Exp>(init)->gen_store_inst(element_addr, block);
            }
        }
    }
}
}
