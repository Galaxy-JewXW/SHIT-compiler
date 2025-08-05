#include "Backend/InstructionSets/RISC-V/Opt/Arithmetic.h"


namespace RISCV::Opt {

// 全局映射：常量 -> 乘法计划
static std::unordered_map<int32_t, std::shared_ptr<MulOp>> operandMap;
static std::vector<std::shared_ptr<Backend::LIR::Instruction>> steps;
static bool initialized = false;
static const int MUL_COST = 3;

// ConstantMulOp 实现
ConstantMulOp::ConstantMulOp(int32_t v) {
    value = v;
    cost = 0;
}

// VariableMulOp 单例
VariableMulOp::VariableMulOp() { cost = 0; }
std::shared_ptr<VariableMulOp> VariableMulOp::getInstance() {
    static VariableMulOp inst;
    return std::make_shared<VariableMulOp>(inst);
}

// MulFinal 单例
MulFinal::MulFinal() { cost = INT_MAX; }
std::shared_ptr<MulFinal> MulFinal::getInstance() {
    static MulFinal inst;
    return std::make_shared<MulFinal>(inst);
}

// Action 构造
Action::Action(Type t, const std::shared_ptr<MulOp> &l, const std::shared_ptr<MulOp> &r) {
    type = t;
    L = l;
    R = r;
    cost = l->cost + r->cost + 1;
}

void ArithmeticOpt::tryAddOp(
        std::shared_ptr<std::vector<std::pair<int32_t, std::shared_ptr<MulOp>>>> &levelLdOperations, int32_t idAdd,
        const std::shared_ptr<MulOp> &odAdd) {
    if (!operandMap.count(idAdd)) {
        operandMap[idAdd] = odAdd;
        levelLdOperations->emplace_back(idAdd, odAdd);
    }
}

// 初始化乘法计划表
void ArithmeticOpt::initialize() {
    if (initialized)
        return;
    std::vector<std::shared_ptr<std::vector<std::pair<int32_t, std::shared_ptr<MulOp>>>>> operations;
    auto level0 = std::make_shared<std::vector<std::pair<int32_t, std::shared_ptr<MulOp>>>>();
    tryAddOp(level0, 0, std::make_shared<ConstantMulOp>(0));
    tryAddOp(level0, 1, VariableMulOp::getInstance());
    operations.push_back(level0);
    // ld -> level dest
    // id -> integer dest
    // i1 -> integer 1
    // o1 -> operand 1
    for (int ld = 1; ld <= MUL_COST; ld++) {
        auto levelLdOperations = std::make_shared<std::vector<std::pair<int32_t, std::shared_ptr<MulOp>>>>();
        for (int l1 = 0; l1 < ld; l1++) {
            // SHIFT LEFT
            for (auto &pair1: *operations[l1]) {
                int32_t i1 = pair1.first;
                std::shared_ptr<MulOp> o1 = pair1.second;
                if (i1 == 0)
                    continue;
                for (int i = 0;; i++) {
                    int32_t idShl = i1 << i;
                    if (i != 0) {
                        auto odShl =
                                std::make_shared<Action>(Action::Type::SHL, o1, std::make_shared<ConstantMulOp>(i));
                        tryAddOp(levelLdOperations, idShl, odShl);
                    }
                    if ((idShl & (1LL << 63)) != 0)
                        break;
                }
            }
            for (int l2 = 0; l1 + l2 < ld; l2++) {
                // ADD, SUB
                for (auto &pair1: *operations[l1]) {
                    int32_t i1 = pair1.first;
                    std::shared_ptr<MulOp> o1 = pair1.second;
                    for (auto &pair2: *operations[l2]) {
                        int32_t i2 = pair2.first;
                        std::shared_ptr<MulOp> o2 = pair2.second;
                        int32_t idAdd = i1 + i2;
                        auto odAdd = std::make_shared<Action>(Action::Type::ADD, o1, o2);
                        tryAddOp(levelLdOperations, idAdd, odAdd);
                        int32_t idSub = i1 - i2;
                        auto odSub = std::make_shared<Action>(Action::Type::SUB, o1, o2);
                        tryAddOp(levelLdOperations, idSub, odSub);
                    }
                }
            }
        }
        operations.push_back(levelLdOperations);
    }
    for (auto &operationList: operations) {
        for (auto &pair: *operationList) {
            operandMap[pair.first] = pair.second;
        }
    }
    initialized = true;
}

// 获取乘法计划
std::shared_ptr<MulOp> ArithmeticOpt::makePlan(int32_t C) {
    if (!initialized)
        initialize();
    auto it = operandMap.find(C);
    return it == operandMap.end() ? MulFinal::getInstance() : it->second;
}

// 在 LIR block 中生成 ans = src * C 的优化序列
std::shared_ptr<Backend::Variable> ArithmeticOpt::readPlanRec(const std::shared_ptr<Backend::LIR::Block> &block,
                                                              const std::shared_ptr<Backend::Variable> &src,
                                                              const std::shared_ptr<Action> &action) {
    std::shared_ptr<MulOp> left = action->L;
    std::shared_ptr<MulOp> right = action->R;
    auto mulAssist = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("mulAssist"), src->workload_type,
                                                         Backend::VariableWide::LOCAL);
    block->parent_function.lock()->add_variable(mulAssist);
    if (action->type == Action::Type::SHL) {
        // 用 dynamic_pointer_cast 判断 left 是否 VariableMulOp
        if (std::dynamic_pointer_cast<VariableMulOp>(left)) {
            auto c = std::dynamic_pointer_cast<ConstantMulOp>(right);
            auto rhs = std::make_shared<Backend::IntValue>(c->value);
            steps.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::SHIFT_LEFT,
                                                                          src, rhs, mulAssist));
        } else {
            // 否则 left 必须是 Action
            auto a = std::dynamic_pointer_cast<Action>(left);
            auto other = readPlanRec(block, src, a);
            block->parent_function.lock()->add_variable(other);
            auto c = std::dynamic_pointer_cast<ConstantMulOp>(right);
            auto rhs = std::make_shared<Backend::IntValue>(c->value);
            steps.push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::SHIFT_LEFT,
                                                                          other, rhs, mulAssist));
        }
    } else if (action->type == Action::Type::SUB || action->type == Action::Type::ADD) {
        // 如果是SUB或者是ADD,那么两边的操作必定都是寄存器,不可能是数字
        auto lc = std::dynamic_pointer_cast<ConstantMulOp>(left);
        auto rc = std::dynamic_pointer_cast<ConstantMulOp>(right);
        std::shared_ptr<Backend::Variable> l, r;
        bool constFlag_l = false;
        bool constFlag_r = false;
        if (lc) {
            //            l = std::make_shared<Backend::IntValue>(0);
            constFlag_l = true;
        } else if (std::dynamic_pointer_cast<Action>(left)) {
            l = readPlanRec(block, src, std::dynamic_pointer_cast<Action>(left));
        } else {
            l = src;
        }
        if (rc) {
            //            r = std::make_shared<Backend::IntValue>(0);
            constFlag_r = true;
        } else if (auto ar = std::dynamic_pointer_cast<Action>(right)) {
            r = readPlanRec(block, src, ar);
        } else {
            r = src;
        }
        auto ty = (action->type == Action::Type::ADD ? Backend::LIR::InstructionType::ADD
                                                     : Backend::LIR::InstructionType::SUB);
        if (constFlag_l & constFlag_r) {
            steps.push_back(
                    std::make_shared<Backend::LIR::LoadIntImm>(mulAssist, std::make_shared<Backend::IntValue>(0)));
        } else if (constFlag_l) {
            if (ty == Backend::LIR::InstructionType::SUB) {
                auto zero_var = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("mulzero"),
                                                                    src->workload_type, Backend::VariableWide::LOCAL);
                block->parent_function.lock()->add_variable(zero_var);
                steps.push_back(
                        std::make_shared<Backend::LIR::LoadIntImm>(zero_var, std::make_shared<Backend::IntValue>(0)));
                steps.push_back(std::make_shared<Backend::LIR::IntArithmetic>(ty, zero_var, r, mulAssist));

            } else {
                steps.push_back(std::make_shared<Backend::LIR::IntArithmetic>(
                        ty, r, std::make_shared<Backend::IntValue>(0), mulAssist));
            }
            block->parent_function.lock()->add_variable(r);
        } else if (constFlag_r) {
            steps.push_back(std::make_shared<Backend::LIR::IntArithmetic>(ty, l, std::make_shared<Backend::IntValue>(0),
                                                                          mulAssist));
            block->parent_function.lock()->add_variable(l);
        } else {
            steps.push_back(std::make_shared<Backend::LIR::IntArithmetic>(ty, l, r, mulAssist));
            block->parent_function.lock()->add_variable(l);
            block->parent_function.lock()->add_variable(r);
        }
    }
    return mulAssist;
}

void ArithmeticOpt::applyMulConst(
        const std::shared_ptr<Backend::LIR::Block> &block,
        const std::shared_ptr<std::vector<std::shared_ptr<Backend::LIR::Instruction>>> &instructions,
        const std::shared_ptr<Backend::Variable> &ans, const std::shared_ptr<Backend::Variable> &src, int32_t C) {
    steps.clear();
    auto mulOp = makePlan(C);
    if (std::dynamic_pointer_cast<MulFinal>(mulOp)) {
        auto tmp = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("mulAssist_tmp"), src->workload_type,
                                                       Backend::VariableWide::LOCAL);
        block->parent_function.lock()->add_variable(tmp);
        steps.push_back(std::make_shared<Backend::LIR::LoadIntImm>(tmp, std::make_shared<Backend::IntValue>(C)));
        steps.push_back(
                std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::MUL, src, tmp, ans));
    } else if (std::dynamic_pointer_cast<VariableMulOp>(mulOp)) {
        steps.push_back(std::make_shared<Backend::LIR::Move>(src, ans));
    } else if (auto cp = std::dynamic_pointer_cast<ConstantMulOp>(mulOp); cp && cp->value == 0) {
        steps.push_back(std::make_shared<Backend::LIR::LoadIntImm>(ans, std::make_shared<Backend::IntValue>(0)));
    } else if (auto act = std::dynamic_pointer_cast<Action>(mulOp)) {
        auto reg = readPlanRec(block, src, act);
        block->parent_function.lock()->add_variable(reg);
        steps.push_back(std::make_shared<Backend::LIR::Move>(reg, ans));
    }
    for (const auto &inst: steps) {
        instructions->push_back(inst);
    }
}

bool DivRemOpt::isPowerOf2(int32_t x) { return x > 0 && (x & (x - 1)) == 0; }

int32_t DivRemOpt::numberOfLeadingZeros(int32_t i) {
    if (i <= 0)
        return i == 0 ? 32 : 0;
    int32_t n = 31;
    if (i >= 1 << 16) {
        n -= 16;
        i >>= 16;
    }
    if (i >= 1 << 8) {
        n -= 8;
        i >>= 8;
    }
    if (i >= 1 << 4) {
        n -= 4;
        i >>= 4;
    }
    if (i >= 1 << 2) {
        n -= 2;
        i >>= 2;
    }
    return n - (i >> 1);
}

int32_t DivRemOpt::log2floor(int32_t x) { return 31 - DivRemOpt::numberOfLeadingZeros(x); }

bool DivRemOpt::applyDivConst(
        const std::shared_ptr<Backend::LIR::Block> &block,
        const std::shared_ptr<std::vector<std::shared_ptr<Backend::LIR::Instruction>>> &instructions,
        const std::shared_ptr<Backend::Variable> &ans, const std::shared_ptr<Backend::Variable> &src, int32_t C) {
    bool isDivisorNeg = C < 0;
    int32_t divisor = isDivisorNeg ? -C : C;
    auto newAns = isDivisorNeg ? std::make_shared<Backend::Variable>(Backend::Utils::unique_name("divTmp"),
                                                                     src->workload_type, Backend::VariableWide::LOCAL)
                               : ans;
    if (isDivisorNeg)
        block->parent_function.lock()->add_variable(newAns);
    if (isPowerOf2(divisor)) {
        int32_t x = log2floor(divisor);
        if (x == 0) {
            instructions->push_back(std::make_shared<Backend::LIR::Move>(src, newAns));
        } else if (x >= 1 && x <= 30) {
            // %1 = srli %src, #(64-x)
            // %2 = addw %src, %1
            // %ans = sraiw %2, #x
            auto op1 = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("divAssist"), src->workload_type,
                                                           Backend::VariableWide::LOCAL);
            block->parent_function.lock()->add_variable(op1);
            auto op2 = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("divAssist"), src->workload_type,
                                                           Backend::VariableWide::LOCAL);
            block->parent_function.lock()->add_variable(op2);
            instructions->push_back(std::make_shared<Backend::LIR::IntArithmetic>(
                    Backend::LIR::InstructionType::SHIFT_RIGHT, src, std::make_shared<Backend::IntValue>(64 - x), op1));
            instructions->push_back(
                    std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::ADD, src, op1, op2));
            instructions->push_back(std::make_shared<Backend::LIR::IntArithmetic>(
                    Backend::LIR::InstructionType::SHIFT_RIGHT, op2, std::make_shared<Backend::IntValue>(x), newAns));
        } else {
            return false;
        }
    } else {
        int32_t l = log2floor(divisor);
        int32_t sh = l;
        int64_t temp = 1LL;
        int64_t low = (temp << (32 + l)) / divisor;
        int64_t high = ((temp << (32 + l)) + (temp << (l + 1))) / divisor;
        while (((low / 2) < (high / 2)) && sh > 0) {
            low /= 2;
            high /= 2;
            sh--;
        }
        if (high < (1LL << 31)) {
            // %1 = mul %src, #high
            // %2 = srai %1, #(32+sh)
            // %3 = sraiw %src, #31
            // %ans = subw %2, %3
            auto op1 = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("divAssist"), src->workload_type,
                                                           Backend::VariableWide::LOCAL);
            block->parent_function.lock()->add_variable(op1);
            auto op2 = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("divAssist"), src->workload_type,
                                                           Backend::VariableWide::LOCAL);
            block->parent_function.lock()->add_variable(op2);
            auto op3 = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("divAssist"), src->workload_type,
                                                           Backend::VariableWide::LOCAL);
            block->parent_function.lock()->add_variable(op3);
            auto tmp = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("divAssist"), src->workload_type,
                                                           Backend::VariableWide::LOCAL);
            block->parent_function.lock()->add_variable(tmp);
            instructions->push_back(
                    std::make_shared<Backend::LIR::LoadIntImm>(tmp, std::make_shared<Backend::IntValue>(high)));
            instructions->push_back(std::make_shared<Backend::LIR::IntArithmetic>(
                    Backend::LIR::InstructionType::MULH_SUP, src, tmp, op1));
            instructions->push_back(
                    std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::SHIFT_RIGHT, op1,
                                                                  std::make_shared<Backend::IntValue>(32 + sh), op2));
            instructions->push_back(std::make_shared<Backend::LIR::IntArithmetic>(
                    Backend::LIR::InstructionType::SHIFT_RIGHT, src, std::make_shared<Backend::IntValue>(31), op3));
            instructions->push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::SUB,
                                                                                  op2, op3, newAns));
        } else {
            high = high - (1LL << 32);
            // %1 = mul %src, #high
            // %2 = srai %1, #32
            // %3 = addw %2, %src
            // %4 = sariw %3, #sh
            // %5 = sariw %src, #31
            // %ans = subw %4, %5
            auto op1 = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("divAssist"), src->workload_type,
                                                           Backend::VariableWide::LOCAL);
            block->parent_function.lock()->add_variable(op1);
            auto op2 = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("divAssist"), src->workload_type,
                                                           Backend::VariableWide::LOCAL);
            block->parent_function.lock()->add_variable(op2);
            auto op3 = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("divAssist"), src->workload_type,
                                                           Backend::VariableWide::LOCAL);
            block->parent_function.lock()->add_variable(op3);
            auto op4 = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("divAssist"), src->workload_type,
                                                           Backend::VariableWide::LOCAL);
            block->parent_function.lock()->add_variable(op4);
            auto op5 = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("divAssist"), src->workload_type,
                                                           Backend::VariableWide::LOCAL);
            block->parent_function.lock()->add_variable(op5);
            auto tmp = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("divAssist"), src->workload_type,
                                                           Backend::VariableWide::LOCAL);
            block->parent_function.lock()->add_variable(tmp);
            instructions->push_back(
                    std::make_shared<Backend::LIR::LoadIntImm>(tmp, std::make_shared<Backend::IntValue>(high)));
            instructions->push_back(std::make_shared<Backend::LIR::IntArithmetic>(
                    Backend::LIR::InstructionType::MULH_SUP, src, tmp, op1));
            instructions->push_back(std::make_shared<Backend::LIR::IntArithmetic>(
                    Backend::LIR::InstructionType::SHIFT_RIGHT, op1, std::make_shared<Backend::IntValue>(32), op2));
            instructions->push_back(
                    std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::ADD, op2, src, op3));
            instructions->push_back(std::make_shared<Backend::LIR::IntArithmetic>(
                    Backend::LIR::InstructionType::SHIFT_RIGHT, op3, std::make_shared<Backend::IntValue>(sh), op4));
            instructions->push_back(std::make_shared<Backend::LIR::IntArithmetic>(
                    Backend::LIR::InstructionType::SHIFT_RIGHT, src, std::make_shared<Backend::IntValue>(31), op5));
            instructions->push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::SUB,
                                                                                  op4, op5, newAns));
        }
    }
    if (isDivisorNeg) {
        std::make_shared<Backend::LIR::LoadIntImm>(ans, std::make_shared<Backend::IntValue>(0));
        instructions->push_back(
                std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::SUB, ans, newAns, ans));
    }
    return true;
}

// TODO:再观察一下，需要测试
void DivRemOpt::applyRemConst(
        const std::shared_ptr<Backend::LIR::Block> &block,
        const std::shared_ptr<std::vector<std::shared_ptr<Backend::LIR::Instruction>>> &instructions,
        const std::shared_ptr<Backend::Variable> &ans, const std::shared_ptr<Backend::Variable> &src, int32_t C) {

//    int32_t temp = std::abs(C);
//    if (isPowerOf2(temp) && C > 0) {
//        //        int32_t shift = log2floor(temp);
//        //        // t0 = src >> 31 (sraiw)
//        //        auto imm31 = std::make_shared<Backend::IntValue>(31);
//        //        auto reg = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("remAssist"),
//        //        src->workload_type,
//        //                                                       Backend::VariableWide::LOCAL);
//        //        block->parent_function.lock()->add_variable(reg);
//        //        instructions->push_back(std::make_shared<Backend::LIR::IntArithmetic>(
//        //                Backend::LIR::InstructionType::SHIFT_RIGHT, src, imm31, reg));
//        //        // reg = reg >> (32 - shift) (srli)
//        //        auto imm1 = std::make_shared<Backend::IntValue>(32 - shift);
//        //        instructions->push_back(std::make_shared<Backend::LIR::IntArithmetic>(
//        //                Backend::LIR::InstructionType::SHIFT_RIGHT, reg, imm1, reg));
//        //        // reg = src + reg (add)
//        //        instructions->push_back(
//        //                std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::ADD, src, reg,
//        //                reg));
//        //        // reg = reg >> shift (srli)
//        //        auto imm2 = std::make_shared<Backend::IntValue>(shift);
//        //        instructions->push_back(std::make_shared<Backend::LIR::IntArithmetic>(
//        //                Backend::LIR::InstructionType::SHIFT_RIGHT, reg, imm2, reg));
//        //        // reg = reg << shift (slli)
//        //        instructions->push_back(std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::SHIFT_LEFT,
//        //                                                                              reg, imm2, reg));
//        //        // ans = src - reg (sub)
//        //        instructions->push_back(
//        //                std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::SUB, src, reg,
//        //                ans));
//        if (temp - 1 >= 2047) {
//            auto reg = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("remAssist"), src->workload_type,
//                                                           Backend::VariableWide::LOCAL);
//            block->parent_function.lock()->add_variable(reg);
//            instructions->push_back(
//                    std::make_shared<Backend::LIR::LoadIntImm>(reg, std::make_shared<Backend::IntValue>(temp - 1)));
//            instructions->push_back(std::make_shared<Backend::LIR::IntArithmetic>(
//                    Backend::LIR::InstructionType::BITWISE_AND, src, reg, ans));
//        } else {
//            instructions->push_back(
//                    std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::BITWISE_AND, src,
//                                                                  std::make_shared<Backend::IntValue>(temp - 1), ans));
//        }
//    } else {
        //        auto q = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("remAssist"),
        //        src->workload_type,
        //                                                     Backend::VariableWide::LOCAL);
        //        block->parent_function.lock()->add_variable(q);
        //        applyDivConst(block, instructions, q, src, C);
        //        auto mulTmp = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("remAssist"),
        //        src->workload_type,
        //                                                          Backend::VariableWide::LOCAL);
        //        block->parent_function.lock()->add_variable(mulTmp);
        //        ArithmeticOpt::applyMulConst(block, instructions, mulTmp, q, C);
        //        instructions->push_back(
        //                std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::SUB, src, mulTmp,
        //                ans));
        auto q = std::make_shared<Backend::Variable>(Backend::Utils::unique_name("remAssist"), src->workload_type,
                                                     Backend::VariableWide::LOCAL);
        block->parent_function.lock()->add_variable(q);
        instructions->push_back(std::make_shared<Backend::LIR::LoadIntImm>(q, std::make_shared<Backend::IntValue>(C)));
        instructions->push_back(
                std::make_shared<Backend::LIR::IntArithmetic>(Backend::LIR::InstructionType::MOD, src, q, ans));
//    }
}

ConstOpt::ConstOpt(const std::shared_ptr<Backend::LIR::Module> &module) { this->module = module; }

ConstOpt::~ConstOpt() { this->module = nullptr; }

void ConstOpt::optimize() {
    for (auto &func: module->functions) {
        if (func->function_type == Backend::LIR::FunctionType::PRIVILEGED) {
            continue;
        }
        for (auto &block: func->blocks) {
            auto newInsts = std::make_shared<std::vector<std::shared_ptr<Backend::LIR::Instruction>>>();
            for (auto &inst: block->instructions) {
                if (auto arithmetic = std::dynamic_pointer_cast<Backend::LIR::IntArithmetic>(inst)) {
                    if (auto C = std::dynamic_pointer_cast<Backend::IntValue>(arithmetic->rhs)) {
                        if (arithmetic->type == Backend::LIR::InstructionType::MUL) {
                            ArithmeticOpt::applyMulConst(block, newInsts, arithmetic->result, arithmetic->lhs,
                                                         C->int32_value);
                        } else if (arithmetic->type == Backend::LIR::InstructionType::DIV) {
                            DivRemOpt::applyDivConst(block, newInsts, arithmetic->result, arithmetic->lhs,
                                                     C->int32_value);
                        } else if (arithmetic->type == Backend::LIR::InstructionType::MOD) {
                            DivRemOpt::applyRemConst(block, newInsts, arithmetic->result, arithmetic->lhs,
                                                     C->int32_value);
                        } else {
                            newInsts->push_back(inst);
                        }
                    } else {
                        newInsts->push_back(inst);
                    }
                } else {
                    newInsts->push_back(inst);
                }
            }
            // 替换 block->instructions
            block->instructions = *newInsts;
        }
    }
}

} // namespace RISCV::Opt
