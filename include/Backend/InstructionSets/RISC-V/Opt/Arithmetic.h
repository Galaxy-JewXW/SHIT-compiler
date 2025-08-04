#ifndef RV_OPT_ARITHMETIC_H
#define RV_OPT_ARITHMETIC_H

#include "Backend/InstructionSets/RISC-V/Instructions.h"
#include "Backend/InstructionSets/RISC-V/Modules.h"
#include "Backend/LIR/DataSection.h"
#include "Backend/LIR/Instructions.h"
#include "Backend/LIR/LIR.h"
#include "Backend/Value.h"

#include <climits>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace RISCV::Opt {

/// 抽象乘法优化计划基类
class MulOp {
public:
    int cost;
    virtual ~MulOp() = default;
};

/// 常量乘法节点（乘以固定值）
class ConstantMulOp : public MulOp {
public:
    int32_t value;
    explicit ConstantMulOp(int32_t v);
};

/// 变量乘法节点（乘以 1）
class VariableMulOp : public MulOp {
public:
    static std::shared_ptr<VariableMulOp> getInstance();

private:
    VariableMulOp();
};

/// 超出阈值时回退使用硬件乘法
class MulFinal : public MulOp {
public:
    static std::shared_ptr<MulFinal> getInstance();

private:
    MulFinal();
};

/// 由两棵子计划和一种操作类型（SHL/ADD/SUB）组成的复合节点
class Action : public MulOp {
public:
    enum Type { SHL, ADD, SUB } type;
    std::shared_ptr<MulOp> L, R;
    Action(Type t, const std::shared_ptr<MulOp> &l, const std::shared_ptr<MulOp> &r);
};

/// 乘法优化主类：构建计划并应用到 LIR Block
class ArithmeticOpt {
public:
    static void tryAddOp(std::shared_ptr<std::vector<std::pair<int32_t, std::shared_ptr<MulOp>>>> &levelLdOperations,
                         int32_t idAdd, const std::shared_ptr<MulOp> &odAdd);

    /// 构建所有小于等于 MUL_COST 的常量乘法计划表
    static void initialize();

    /// 返回给定常量 C 的最优乘法计划（可能是 MulFinal）
    static std::shared_ptr<MulOp> makePlan(int32_t C);

    static std::shared_ptr<Backend::Variable> readPlanRec(const std::shared_ptr<Backend::LIR::Block> &block,
                                                          const std::shared_ptr<Backend::Variable> &src,
                                                          const std::shared_ptr<Action> &action);

    /// 在 LIR block 中生成 ans = src * C 的优化指令序列
    static void applyMulConst(const std::shared_ptr<Backend::LIR::Block> &block,
                  const std::shared_ptr<std::vector<std::shared_ptr<Backend::LIR::Instruction>>> &instructions,
                  const std::shared_ptr<Backend::Variable> &ans, const std::shared_ptr<Backend::Variable> &src,
                  int32_t C);
};

/*------------------------------除法取余-------------------------------------*/

class DivRemOpt {
public:
    // 在 LIR block 中生成 ans = src / C 的优化序列
    static bool
    applyDivConst(const std::shared_ptr<Backend::LIR::Block> &block,
                  const std::shared_ptr<std::vector<std::shared_ptr<Backend::LIR::Instruction>>> &instructions,
                  const std::shared_ptr<Backend::Variable> &ans, const std::shared_ptr<Backend::Variable> &src,
                  int32_t C);

    // 在 LIR block 中生成 ans = src % C 的优化序列
    static void
    applyRemConst(const std::shared_ptr<Backend::LIR::Block> &block,
                  const std::shared_ptr<std::vector<std::shared_ptr<Backend::LIR::Instruction>>> &instructions,
                  const std::shared_ptr<Backend::Variable> &ans, const std::shared_ptr<Backend::Variable> &src,
                  int32_t C);

    static bool isPowerOf2(int32_t x);
    static int32_t log2floor(int32_t x);
    static int32_t numberOfLeadingZeros(int32_t i);
};

class ConstOpt {
public:
    ConstOpt(const std::shared_ptr<Backend::LIR::Module> &module);
    ~ConstOpt();
    std::shared_ptr<Backend::LIR::Module> module;
    void optimize();
};

} // namespace RISCV::Opt

#endif
