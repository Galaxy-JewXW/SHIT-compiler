#ifndef BACKEND_DATA_SECTION_H
#define BACKEND_DATA_SECTION_H

#include <sstream>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "Utils/Log.h"
#include "Backend/VariableTypes.h"
#include "Backend/Value.h"
#include "Mir/Structure.h"
#include "Mir/Init.h"
#include "Mir/Type.h"

namespace Backend {
    class DataSection;
}

class Backend::DataSection {
    public:
        class Variable : public Backend::Variable {
            public:
                class InitValue {
                    public:
                        enum class Type : uint32_t {
                            STRING, CONSTANTS
                        };
                        Type value_type;
                        explicit InitValue(Type value_type) : value_type(value_type) {};
                };

                class ConstString : public InitValue {
                    public:
                        explicit ConstString(std::string &str) : InitValue(Type::STRING), str(std::move(str)) {};
                        std::string str;
                };

                class Constants : public InitValue {
                    public:
                        explicit Constants(std::vector<std::shared_ptr<Mir::Init::Constant>> &constants) : InitValue(Type::CONSTANTS), constants(std::move(constants)) {}
                        std::vector<std::shared_ptr<Mir::Init::Constant>> constants;
                };

                bool read_only{false};
                std::shared_ptr<InitValue> init_value;

                inline void load_from_llvm(const std::shared_ptr<Mir::Init::Constant> &value) {
                    this->init_value = std::make_shared<Variable::Constants>(std::vector<std::shared_ptr<Mir::Init::Constant>>{value});
                }

                inline void load_from_llvm(const Mir::Init::Array &value)  {
                    length = value.get_size();
                    if (value.zero_initialized()) {
                        this->init_value = std::make_shared<Variable::Constants>(std::vector<std::shared_ptr<Mir::Init::Constant>>{});
                    } else {
                        std::vector<std::shared_ptr<Mir::Init::Constant>> constants;
                        for (size_t i = 0; i < length; i++) {
                            std::shared_ptr<Mir::Init::Init> element = const_cast<Mir::Init::Array&>(value).get_init_value({static_cast<int>(i)});
                            constants.push_back(std::static_pointer_cast<Mir::Init::Constant>(element));
                        }
                        this->init_value = std::make_shared<Variable::Constants>(constants);
                    }
                }
                explicit Variable(const std::string &name, const Backend::VariableType &type) : Backend::Variable(name, type, Backend::VariableWide::GLOBAL) {};
        };

        std::unordered_map<std::string, std::shared_ptr<Variable>> global_variables;

        inline void load_global_variables(const std::vector<std::shared_ptr<Mir::GlobalVariable>> &global_variables) {
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

        inline void load_global_variables(const std::shared_ptr<std::vector<std::string>> &const_strings) {
            for (size_t i = 0; i < const_strings->size(); i++) {
                std::shared_ptr<Backend::DataSection::Variable> var = std::make_shared<Backend::DataSection::Variable>("str_" + std::to_string(i + 1), Backend::VariableType::STRING);
                var->read_only = true;
                var->init_value = std::make_shared<Variable::ConstString>((*const_strings)[i]);
                this->global_variables[var->name] = var;
            }
        }
};

#endif