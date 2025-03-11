#include <unordered_set>

#include "Mir/Builder.h"
#include "Pass/Analysis.h"
#include "Pass/Transform.h"

namespace Pass {

void LCSSA::transform(std::shared_ptr<Mir::Module> module) {

    for (auto &fun: *module) {

    }
}
}
