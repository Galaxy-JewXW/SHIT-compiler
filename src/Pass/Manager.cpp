#include "Pass/Analysis.h"
#include "Pass/Transform.h"
#include "Pass/Utility.h"

void execute_O0_passes(std::shared_ptr<Mir::Module> &module) {
    const auto example_pass = Pass::Pass::create<Pass::Example>();
    module = module | example_pass;
}
