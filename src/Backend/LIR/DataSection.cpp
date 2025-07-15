#include "Backend/LIR/DataSection.h"

// std::string Backend::DataSection::to_string() const {
//     std::ostringstream oss;
//     oss << ".section .rodata\n";
//     for (const std::shared_ptr<Backend::DataSection::Variable> &var : global_variables) {
//         if (var->read_only)
//             oss << var->to_string() << "\n";
//     }
//     oss << ".section .data\n";
//     for (const std::shared_ptr<Backend::DataSection::Variable> &var : global_variables) {
//         if (!var->read_only)
//             oss << var->to_string() << "\n";
//     }
//     oss << "# END OF DATA FIELD\n";
//     return oss.str();
// }