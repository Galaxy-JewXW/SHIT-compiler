#ifndef BACKEND_DATA_SECTION_H
#define BACKEND_DATA_SECTION_H

#include <sstream>
#include <vector>
#include <memory>
#include <string>
#include "Utils/Log.h"
#include "Backend/VariableTypes.h"
#include "Mir/Structure.h"
#include "Mir/Init.h"
#include "Mir/Type.h"

namespace Backend {
    class DataSection;
}

class Backend::DataSection {
    public:
        class Variable {
            public:
                std::string name;
                Backend::VariableType type;
                std::string init_value{""};
                bool read_only{false};

                [[nodiscard]] std::string to_string() const;

                void load_from_llvm(const Mir::Init::Init &value);

                void load_from_llvm(const Mir::Init::Array &value, const size_t len);

                explicit Variable(const std::string &name, const Backend::VariableType &type) : name(name), type(type) {};
        };

        std::vector<std::shared_ptr<Variable>> global_variables;

        [[nodiscard]] std::string to_string() const;

        void load_global_variables(const std::vector<std::shared_ptr<Mir::GlobalVariable>> &global_variables);

        void load_global_variables(const std::shared_ptr<std::vector<std::string>> &const_strings);
};

#endif