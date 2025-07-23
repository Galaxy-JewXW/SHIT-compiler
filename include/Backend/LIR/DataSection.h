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
                        explicit Constants(std::vector<std::shared_ptr<Backend::Constant>> &&constants) : InitValue(Type::CONSTANTS), constants(std::move(constants)) {}
                        std::vector<std::shared_ptr<Backend::Constant>> constants;
                };

                bool read_only{false};
                std::shared_ptr<InitValue> init_value;

                void load_from_llvm(const std::shared_ptr<Mir::Init::Constant> &value);
                void load_from_llvm(const Mir::Init::Array &value);

                [[nodiscard]] std::string label() const override { return name.substr(1); }

                explicit Variable(const std::string &name, const Backend::VariableType &type) : Backend::Variable(name, type, Backend::VariableWide::GLOBAL) {};
            private:
                /*
                 * Convert a `Mir::Init::Constant` value to `Backend::Constant`.
                 * Support only `ConstInt` and `ConstFloat`.
                 */
                std::shared_ptr<Backend::Constant> load_from_llvm_(const std::shared_ptr<Mir::Init::Constant> &value);
        };

        std::unordered_map<std::string, std::shared_ptr<Variable>> global_variables;

        void load_global_variables(const std::vector<std::shared_ptr<Mir::GlobalVariable>> &global_variables);
        void load_global_variables(const std::shared_ptr<std::vector<std::string>> &const_strings);
};

#endif