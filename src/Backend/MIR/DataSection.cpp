#include "Backend/MIR/DataSection.h"

std::string Backend::DataSection::to_string() const {
    std::ostringstream oss;
    oss << ".section .rodata\n";
    for (const std::shared_ptr<Backend::DataSection::Variable> &var : this->global_variables) {
        if (var->read_only)
            oss << var->to_string() << "\n";
    }
    oss << ".section .data\n";
    for (const std::shared_ptr<Backend::DataSection::Variable> &var : this->global_variables) {
        if (!var->read_only)
            oss << var->to_string() << "\n";
    }
    oss << "# END OF DATA FIELD\n";
    return oss.str();
}

void Backend::DataSection::load_global_variables(const std::vector<std::shared_ptr<Mir::GlobalVariable>> &global_variables) {
    for (const std::shared_ptr<Mir::GlobalVariable> &global_variable : global_variables) {
        Mir::Init::Init *init_value = global_variable->get_init_value().get();
        Backend::VariableType type = Backend::Utils::llvm_to_riscv(*init_value->get_type());
        std::shared_ptr<Backend::DataSection::Variable> var = std::make_shared<Backend::DataSection::Variable>(global_variable->get_name(), type);
        if (Backend::Utils::is_pointer(type))
            var->load_from_llvm(*dynamic_cast<const Mir::Init::Array *>(init_value), dynamic_cast<const Mir::Type::Array *>(init_value->get_type().get())->get_size());
        else
            var->load_from_llvm(*init_value);
        this->global_variables.push_back(var);
    }
}

void Backend::DataSection::load_global_variables(const std::shared_ptr<std::vector<std::string>> &const_strings) {
    for (size_t i = 0; i < const_strings->size(); i++) {
        std::shared_ptr<Backend::DataSection::Variable> var = std::make_shared<Backend::DataSection::Variable>(".str_" + std::to_string(i + 1), Backend::VariableType::STRING);
        var->read_only = true;
        std::ostringstream oss;
        oss << ".string \"" << (*const_strings)[i] << "\"";
        var->init_value = oss.str();
        this->global_variables.push_back(var);
    }
}

void Backend::DataSection::Variable::load_from_llvm(const Mir::Init::Init &value) {
    if (Backend::Utils::is_pointer(this->type))
        log_error("Init failed because variable %s is an array.", this->name.c_str());
    std::ostringstream oss;
    const Mir::Init::Constant *const_value = dynamic_cast<const Mir::Init::Constant *>(&value);
    oss << Backend::Utils::to_riscv_indicator(type) << " " << const_value->get_const_value()->to_string();
    this->init_value = oss.str();
}

void Backend::DataSection::Variable::load_from_llvm(const Mir::Init::Array &value, const size_t len) {
    std::ostringstream oss;
    Backend::VariableType element_type = Backend::Utils::to_reference(this->type);
    if (value.zero_initialized()) {
        oss << ".zero " << len * Backend::Utils::type_to_size(element_type);
        this->init_value = oss.str();
        return;
    }
    for (size_t i = 0; i < len; i++) {
        if (i > 0) oss << ", ";
        std::vector<int> indexes = {static_cast<int>(i)};
        std::shared_ptr<Mir::Init::Init> element = const_cast<Mir::Init::Array&>(value).get_init_value(indexes);
        if (const Mir::Init::Constant *const_elem = dynamic_cast<const Mir::Init::Constant *>(element.get())) {
            oss << Backend::Utils::to_riscv_indicator(element_type) << " " << const_elem->get_const_value()->to_string();
        } else {
            size_t remaining_elements = len - i;
            size_t element_size = Backend::Utils::type_to_size(element_type);
            oss << ".zero " << remaining_elements * element_size;
            break;
        }
    }
    this->init_value = oss.str();
}

std::string Backend::DataSection::Variable::to_string() const {
    std::ostringstream oss;
    oss << ".global_variable_" << this->name << ": " << this->init_value;
    return oss.str();
}