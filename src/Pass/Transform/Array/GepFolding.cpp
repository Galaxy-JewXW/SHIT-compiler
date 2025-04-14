#include "Pass/Transform.h"
#include "Pass/Util.h"

using namespace Mir;

namespace {
bool is_folded_gep(const std::shared_ptr<Instruction> &instruction) {
    const auto gep = instruction->is<GetElementPtr>();
    if (gep == nullptr) {
        return false;
    }
    const bool is_folded = gep->get_addr()->is<GetElementPtr>() != nullptr;
    log_trace("%d %s", static_cast<int>(is_folded), gep->to_string().c_str());
    return is_folded;
}

void try_fold_gep(const std::shared_ptr<GetElementPtr> &gep) {
    if (gep->users().size() == 0) {
        return;
    }
    const auto block = gep->get_block();
    std::vector<std::shared_ptr<GetElementPtr>> chain;
    std::shared_ptr<Value> current = gep;
    while (const auto cur_gep = current->is<GetElementPtr>()) {
        chain.push_back(cur_gep);
        current = cur_gep->get_addr();
    }
    std::reverse(chain.begin(), chain.end());
    // TODO


}
}

namespace Pass {
void GepFolding::run_on_func(const std::shared_ptr<Function> &func) const {
    std::vector<std::shared_ptr<GetElementPtr>> geps;
    for (const auto &block: cfg->dom_tree_layer(func)) {
        for (const auto &instruction: block->get_instructions()) {
            if (is_folded_gep(instruction)) {
                geps.push_back(instruction->as<GetElementPtr>());
            }
        }
    }
    std::reverse(geps.begin(), geps.end());
    for (const auto &gep: geps) {
        log_trace("%s", gep->to_string().c_str());
    }
    for (const auto &gep: geps) {
        try_fold_gep(gep);
    }
}


void GepFolding::transform(const std::shared_ptr<Module> module) {
    cfg = get_analysis_result<ControlFlowGraph>(module);
    for (const auto &func: *module) {
        run_on_func(func);
    }
    module->update_id();
    cfg = nullptr;
}
}
