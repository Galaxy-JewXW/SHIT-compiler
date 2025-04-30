#include <variant>

#include "Mir/Instruction.h"
#include "Pass/Transforms/DataFlow.h"

using namespace Mir;

namespace {
// 具有交换律和结合律的int/float运算
constexpr bool float_operands_available = false;

using Op = std::variant<IntBinary::Op, FloatBinary::Op>;

std::unordered_map<Op, int> int_ops_priority = {
    {IntBinary::Op::ADD, 0},
    {IntBinary::Op::SUB, 0},
    {IntBinary::Op::MUL, 1},
    {IntBinary::Op::DIV, 1},
    {IntBinary::Op::MOD, 1}
};

std::unordered_map<Op, int> float_ops_priority = {
    {FloatBinary::Op::ADD, 0},
    {FloatBinary::Op::SUB, 0},
    {FloatBinary::Op::MUL, 1},
    {FloatBinary::Op::DIV, 1},
    {FloatBinary::Op::MOD, 1}
};

struct CalcNode {
    std::shared_ptr<CalcNode> left, right, parent;
    std::shared_ptr<Binary> binary;

    CalcNode(const std::shared_ptr<CalcNode> &parent, const std::shared_ptr<CalcNode> &left,
             const std::shared_ptr<CalcNode> &right, const std::shared_ptr<Binary> &binary)
        : left(left), right(right), parent(parent), binary(binary) {}
};
}

namespace Pass {
void TreeHeightBalance::run_on_func(const std::shared_ptr<Function> &func) {
    log_trace("%s", func->get_name().c_str());
}

void TreeHeightBalance::transform(const std::shared_ptr<Module> module) {
    for (const auto &func: *module) {
        run_on_func(func);
    }
}
}
