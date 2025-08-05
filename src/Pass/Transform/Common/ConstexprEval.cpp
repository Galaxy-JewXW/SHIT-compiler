#include "Mir/Eval.h"
#include "Pass/Transforms/Array.h"
#include "Pass/Transforms/Common.h"
#include "Pass/Transforms/DCE.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
size_t size_of_type(const std::shared_ptr<Type::Type> &type) {
    if (type->is_int32()) {
        return sizeof(int);
    }
    if (type->is_float()) {
        return sizeof(double);
    }
    if (type->is_array()) {
        return size_of_type(type->as<Type::Array>()->get_atomic_type()) * type->as<Type::Array>()->get_flattened_size();
    }
    log_error("Invalid type %s", type->to_string().c_str());
}

class ModuleInterpreter {
public:
    explicit ModuleInterpreter(const std::shared_ptr<Module> &module, const std::shared_ptr<Pass::FunctionAnalysis> &func_analysis) :
        module_{module}, func_analysis_{func_analysis} {}

    [[nodiscard]] bool can_run() const;

    void run() const;

private:
    const std::shared_ptr<Module> &module_;
    const std::shared_ptr<Pass::FunctionAnalysis> func_analysis_;
};

bool ModuleInterpreter::can_run() const {
    for (const auto &func: module_->get_functions()) {
        if (func_analysis_->func_info(func).io_read)
            return false;
    }

    if (module_->get_functions().size() > 1)
        return false;

    if (!module_->get_global_variables().empty())
        return false;

    const auto main_func = module_->get_main_function();
    size_t alloc_size = 0;
    for (const auto &block : main_func->get_blocks()) {
        for (const auto &inst : block->get_instructions()) {
            if (inst->get_op() == Operator::ALLOC) {
                const auto alloc = inst->as<Alloc>();
                alloc_size += size_of_type(alloc->get_type()->as<Type::Pointer>()->get_contain_type());
            }
        }
    }

    if (alloc_size >= 100000)
        return false;

    return true;
}

void ModuleInterpreter::run() const {
    const auto main_func = module_->get_main_function();
    if (!can_run()) {
        return;
    }

    const auto cache{std::make_shared<Interpreter::Cache>()};
    const auto interpreter{std::make_unique<Interpreter>(cache, true)};
    try {
        interpreter->interpret_module_mode(main_func);
    } catch (const std::exception &e) {
        log_error("%s", e.what());
    }

    const auto block = Block::create("only", nullptr);
    for (const auto &inst : interpreter->frame->kept) {
        inst->set_block(block);
    }

    main_func->get_blocks().clear();
    block->set_function(main_func);
    const auto ret = interpreter->frame->ret_value;
    Ret::create(ConstInt::create(ret.get<int>()), block);
}
} // namespace

namespace Pass {
template<bool module_mode>
bool ConstexprFuncEval<module_mode>::is_constexpr_func(const std::shared_ptr<Function> &func) const {
    if (func->is_runtime_func()) {
        return false;
    }
    const auto info{func_analysis->func_info(func)};
    if (info.memory_read || info.memory_write || info.memory_alloc) {
        return false;
    }
    if (info.io_read || info.io_write) {
        return false;
    }
    if (info.has_side_effect || !info.no_state) {
        return false;
    }
    if (!info.has_return) {
        return false;
    }
    return true;
}

template<bool module_mode>
bool ConstexprFuncEval<module_mode>::run_on_func(const std::shared_ptr<Function> &func) const {
    bool changed = false;
    const auto cache{std::make_shared<Interpreter::Cache>()};
    for (const auto &block: func->get_blocks()) {
        for (const auto &instruction: block->get_instructions()) {
            if (instruction->get_op() != Operator::CALL) [[likely]] {
                continue;
            }
            const auto call{instruction->as<Call>()};
            const auto called_func{call->get_function()->as<Function>()};
            if (!is_constexpr_func(called_func)) {
                continue;
            }

            const auto &real_params{call->get_params()};
            bool all_params_constant{true};
            const auto real_args = [&]() -> std::vector<eval_t> {
                std::vector<eval_t> args;
                args.reserve(real_params.size());
                for (const auto &param: real_params) {
                    if (!param->is_constant()) {
                        all_params_constant = false;
                        break;
                    }
                    args.emplace_back(param->as<Const>()->get_constant_value());
                }
                return args;
            }();
            if (!all_params_constant) {
                continue;
            }
            eval_t result;
            if (const Interpreter::Key key{called_func->get_name(), real_args}; cache->contains(key)) {
                result = cache->get(key);
            } else {
                const auto interpreter{std::make_unique<Interpreter>(cache)};
                try {
                    interpreter->interpret_function(called_func, real_args);
                } catch (const std::exception &) {
                    continue;
                }
                result = interpreter->frame->ret_value;
                cache->put(key, result);
            }
            const auto new_value = [&]() -> std::shared_ptr<Value> {
                if (called_func->get_return_type()->is_int32()) {
                    return ConstInt::create(result.get<int>());
                }
                if (called_func->get_return_type()->is_float()) {
                    return ConstFloat::create(result.get<double>());
                }
                log_error("Invalid return type %s", called_func->get_return_type()->to_string().c_str());
            }();
            call->replace_by_new_value(new_value);
            changed = true;
            break;
        }
        if (changed) {
            break;
        }
    }
    return changed;
}

template<bool module_mode>
void ConstexprFuncEval<module_mode>::transform(const std::shared_ptr<Module> module) {
    func_analysis = get_analysis_result<FunctionAnalysis>(module);
    bool changed{false};

    do {
        changed = false;
        std::for_each(module->get_functions().begin(), module->get_functions().end(),
                      [&](const auto &func) { changed |= run_on_func(func); });
        if (changed) {
            create<DeadInstEliminate>()->run_on(module);
        }
    } while (changed);

    if constexpr (module_mode) {
        func_analysis = get_analysis_result<FunctionAnalysis>(module);
        if (const ModuleInterpreter module_interpreter{module, func_analysis}; module_interpreter.can_run()) {
            create<GepFolding>()->run_on(module);
            log_debug("\n%s", module->to_string().c_str());
            module_interpreter.run();
        }
    }
    func_analysis = nullptr;
}

template class ConstexprFuncEval<true>;
template class ConstexprFuncEval<false>;
} // namespace Pass
