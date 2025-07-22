#include "Backend/LIR/DataSection.h"

void Backend::DataSection::load_global_variables(const std::vector<std::shared_ptr<Mir::GlobalVariable>> &global_variables) {
    for (const std::shared_ptr<Mir::GlobalVariable> &global_variable : global_variables) {
        std::shared_ptr<Mir::Init::Init> init_value = global_variable->get_init_value();
        Backend::VariableType type = Backend::Utils::llvm_to_riscv(*init_value->get_type());
        std::shared_ptr<Backend::DataSection::Variable> var = std::make_shared<Backend::DataSection::Variable>(global_variable->get_name(), Backend::Utils::to_reference(type));
        if (Backend::Utils::is_pointer(type))
            var->load_from_llvm(*std::static_pointer_cast<const Mir::Init::Array>(init_value));
        else
            var->load_from_llvm(std::static_pointer_cast<Mir::Init::Constant>(init_value));
        this->global_variables[var->name] = var;
    }
}

void Backend::DataSection::load_global_variables(const std::shared_ptr<std::vector<std::string>> &const_strings) {
    for (size_t i = 0; i < const_strings->size(); i++) {
        std::shared_ptr<Backend::DataSection::Variable> var = std::make_shared<Backend::DataSection::Variable>("str_" + std::to_string(i + 1), Backend::VariableType::STRING);
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

void Backend::DataSection::Variable::load_from_llvm(const Mir::Init::Array &value)  {
    length = value.get_size();
    this->length = length;
    this->init_value = std::make_shared<Variable::Constants>(std::vector<std::shared_ptr<Backend::Constant>>{});
    std::shared_ptr<Variable::Constants> init_value = std::static_pointer_cast<Variable::Constants>(this->init_value);
    if (!value.zero_initialized()) {
        size_t last_non_zero = value.last_non_zero();
        for (size_t i = 0; i < last_non_zero; i++) {
            std::shared_ptr<Mir::Init::Init> element = const_cast<Mir::Init::Array&>(value).get_init_value({static_cast<int>(i)});
            init_value->constants.push_back(load_from_llvm_(std::static_pointer_cast<Mir::Init::Constant>(element)));
        }
    }
}