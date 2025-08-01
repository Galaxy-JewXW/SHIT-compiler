#include "Backend/LIR/DataSection.h"

void Backend::DataSection::load_global_variables(const std::vector<std::shared_ptr<Mir::GlobalVariable>> &global_variables) {
    for (const std::shared_ptr<Mir::GlobalVariable> &global_variable : global_variables) {
        std::shared_ptr<Mir::Init::Init> init_value = global_variable->get_init_value();
        Backend::VariableType type = Backend::Utils::llvm_to_riscv(*init_value->get_type());
        std::shared_ptr<Backend::DataSection::Variable> var = std::make_shared<Backend::DataSection::Variable>(global_variable->get_name(), Backend::Utils::to_reference(type));
        if (Backend::Utils::is_pointer(type))
            var->load_from_llvm(std::static_pointer_cast<Mir::Init::Array>(init_value));
        else
            var->load_from_llvm(std::static_pointer_cast<Mir::Init::Constant>(init_value));
        this->global_variables[var->name] = var;
    }
}

void Backend::DataSection::load_global_variables(const std::shared_ptr<std::vector<std::string>> &const_strings) {
    for (size_t i = 0; i < const_strings->size(); i++) {
        std::shared_ptr<Backend::DataSection::Variable> var = std::make_shared<Backend::DataSection::Variable>(std::to_string(i), Backend::VariableType::STRING);
        var->read_only = true;
        var->init_value = std::make_shared<Variable::ConstString>((*const_strings)[i]);
        this->global_variables[var->name] = var;
    }
}

std::shared_ptr<Backend::Constant> Backend::DataSection::Variable::load_from_llvm_(const std::shared_ptr<Mir::Init::Constant> &value) {
    if (value->get_type()->is_int32())
        return std::make_shared<Backend::IntValue>(
            static_cast<int32_t>(
                std::static_pointer_cast<Mir::ConstInt>(
                    std::static_pointer_cast<Mir::Init::Constant>(value)->get_const_value()
                )->get_constant_value()
            )
        );
    else
        return std::make_shared<Backend::FloatValue>(
            static_cast<double>(
                std::static_pointer_cast<Mir::ConstFloat>(
                    std::static_pointer_cast<Mir::Init::Constant>(value)->get_const_value()
                )->get_constant_value()
            )
        );
}

void Backend::DataSection::Variable::load_from_llvm(const std::shared_ptr<Mir::Init::Constant> &value) {
    this->init_value = std::make_shared<Variable::Constants>(std::vector<std::shared_ptr<Backend::Constant>>{load_from_llvm_(value)});
}

void Backend::DataSection::Variable::load_from_llvm(const std::shared_ptr<Mir::Init::Array> &value)  {
    length = value->get_type()->as<Mir::Type::Array>()->get_flattened_size();
    this->init_value = std::make_shared<Variable::Constants>(std::vector<std::shared_ptr<Backend::Constant>>{});
    std::shared_ptr<Variable::Constants> init_value = std::static_pointer_cast<Variable::Constants>(this->init_value);

    const std::shared_ptr<Mir::Type::Type> atomic_type = value->get_type()->as<Mir::Type::Array>()->get_atomic_type();
    if (value->zero_initialized()) {
        if (atomic_type->is_integer()) {
            init_value->constants.push_back(std::make_shared<IntMultiZero>(length));
        } else if (atomic_type->is_float()) {
            init_value->constants.push_back(std::make_shared<FloatMultiZero>(length));
        }
        return;
    }

    auto build = [&](auto &&self, const std::shared_ptr<Mir::Init::Init> &init) -> std::vector<std::shared_ptr<Backend::Constant>> {
        std::vector<std::shared_ptr<Backend::Constant>> res;
        if (init->is_array_init()) {
            const std::shared_ptr<Mir::Init::Array> array = std::static_pointer_cast<Mir::Init::Array>(init);
            const std::shared_ptr<Mir::Type::Array> array_type = array->get_type()->as<Mir::Type::Array>();
            const std::vector<std::shared_ptr<Mir::Init::Init>> &array_init_values = array->get_init_values();
            if (array->zero_initialized()) {
                if (atomic_type->is_integer()) {
                    res.push_back(std::make_shared<IntMultiZero>(array_type->get_flattened_size()));
                } else if (atomic_type->is_float()) {
                    res.push_back(std::make_shared<FloatMultiZero>(array_type->get_flattened_size()));
                }
            } else if (const int p = array->last_non_zero(); p != -1) {
                for (int i = 0; i <= p; ++i) {
                    res.push_back(load_from_llvm_(std::static_pointer_cast<Mir::Init::Constant>(array_init_values[i])));
                }
                const int remaining = static_cast<int>(array->get_size()) - p - 1;
                if (atomic_type->is_integer()) {
                    res.push_back(std::make_shared<IntMultiZero>(remaining));
                } else if (atomic_type->is_float()) {
                    res.push_back(std::make_shared<FloatMultiZero>(remaining));
                }
            } else {
                for (const std::shared_ptr<Mir::Init::Init> &element: array_init_values) {
                    const std::vector<std::shared_ptr<Backend::Constant>> _res = self(self, element);
                    res.insert(res.end(), _res.begin(), _res.end());
                }
            }
        } else if (init->is_constant_init()) {
            res.push_back(load_from_llvm_(std::static_pointer_cast<Mir::Init::Constant>(init)));
        }
        return res;
    };

    const std::vector<std::shared_ptr<Backend::Constant>> res = build(build, value);
    std::vector<std::shared_ptr<Backend::Constant>> constants;
    constants.reserve(res.size());

    if (atomic_type->is_integer()) {
        for (const std::shared_ptr<Backend::Constant> &constant: res) {
            if (const auto multi_zero = std::dynamic_pointer_cast<Backend::IntMultiZero>(constant);
                multi_zero != nullptr && !constants.empty()) {
                if (const auto _back = std::dynamic_pointer_cast<Backend::IntMultiZero>(constants.back())) {
                    constants.back() = std::make_shared<IntMultiZero>(multi_zero->zero_count + _back->zero_count);
                    continue;
                }
            }
            constants.push_back(constant);
        }
    }

    init_value->constants.insert(init_value->constants.end(), constants.begin(), constants.end());
}