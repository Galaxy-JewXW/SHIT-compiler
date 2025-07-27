#include "Pass/Transforms/Common.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
class SimpleReassociateImpl {
public:
    explicit SimpleReassociateImpl(const std::shared_ptr<Function> &current_function) :
        current_function(current_function) {}

    void impl();

private:
    const std::shared_ptr<Function> &current_function;
    std::unordered_set<std::shared_ptr<IntBinary>> worklist{};
    std::unordered_set<std::shared_ptr<IntBinary>> instructions_to_erase{};
};
}


void SimpleReassociateImpl::impl() {}


namespace Pass {
void Reassociate::transform(const std::shared_ptr<Module> module) {
    create<StandardizeBinary>()->run_on(module);
    for (const auto &func: module->get_functions()) {
        SimpleReassociateImpl{func}.impl();
    }
}
}
