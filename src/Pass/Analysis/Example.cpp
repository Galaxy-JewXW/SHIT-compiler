#include "Pass/Analysis.h"
#include "Pass/Transform.h"
#include "Pass/Utility.h"

using namespace Pass;

void Example::analyze(const std::shared_ptr<const Mir::Module> module) {
    log_debug("this module has %d const string(s)", module->get_const_string_size());
}
