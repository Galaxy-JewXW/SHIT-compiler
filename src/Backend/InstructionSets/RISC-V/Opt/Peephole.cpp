#include "Backend/InstructionSets/RISC-V/Opt/Peephole.h"

RISCV::Opt::PeepholeBeforeRA::PeepholeBeforeRA(const std::shared_ptr<Backend::LIR::Module> &module) {
    this->module = module;
}

RISCV::Opt::PeepholeBeforeRA::~PeepholeBeforeRA() { this->module = nullptr; }

void RISCV::Opt::PeepholeBeforeRA::optimize() {
    for (auto &function: module->functions) {
        if (function->function_type == Backend::LIR::FunctionType::PRIVILEGED)
            continue;
        for (auto &block: function->blocks) {
            uselessLoadRemove(block);
            preRAConstValueReUse(block);
            preRAConstPointerReUse(block);
            addZero2Mv(block);
            addiLS2LSOffset(block);
        }
    }
}


void RISCV::Opt::PeepholeBeforeRA::addZero2Mv(const std::shared_ptr<Backend::LIR::Block> &block) {
    for (auto &inst: block->instructions) {
        if (inst->type == Backend::LIR::InstructionType::ADD) {
            auto add = std::static_pointer_cast<Backend::LIR::IntArithmetic>(inst);
            if (add->rhs->operand_type == Backend::OperandType::CONSTANT &&
                std::static_pointer_cast<Backend::IntValue>(add->rhs)->int32_value == 0) {
                inst = std::make_shared<Backend::LIR::Move>(add->result, add->lhs);
            }
        }
    }
}

// TODO:需要活跃变量分析
void RISCV::Opt::PeepholeBeforeRA::addiLS2LSOffset(const std::shared_ptr<Backend::LIR::Block> &block) {}

void RISCV::Opt::PeepholeBeforeRA::uselessLoadRemove(const std::shared_ptr<Backend::LIR::Block> &block) {
    auto &instructions = block->instructions;
    std::vector<decltype(instructions.begin())> toRemove;
    for (auto it = std::next(instructions.begin()); it != instructions.end(); ++it) {
        auto prev = std::prev(it);
        if ((*prev)->type == Backend::LIR::InstructionType::LOAD &&
            (*it)->type == Backend::LIR::InstructionType::LOAD) {
            auto L = std::static_pointer_cast<Backend::LIR::LoadInt>(*prev);
            auto R = std::static_pointer_cast<Backend::LIR::LoadInt>(*it);
            if (L->var_in_mem == R->var_in_mem && L->offset == R->offset && L->var_in_reg == R->var_in_reg)
                toRemove.push_back(it);
        } else if ((*prev)->type == Backend::LIR::InstructionType::STORE &&
                   (*it)->type == Backend::LIR::InstructionType::STORE) {
            auto L = std::static_pointer_cast<Backend::LIR::StoreInt>(*prev);
            auto R = std::static_pointer_cast<Backend::LIR::StoreInt>(*it);
            if (L->var_in_mem == R->var_in_mem && L->offset == R->offset && L->var_in_reg == R->var_in_reg)
                toRemove.push_back(it);
        } else if ((*prev)->type == Backend::LIR::InstructionType::FLOAD &&
                   (*it)->type == Backend::LIR::InstructionType::FLOAD) {
            auto L = std::static_pointer_cast<Backend::LIR::LoadInt>(*prev);
            auto R = std::static_pointer_cast<Backend::LIR::LoadInt>(*it);
            if (L->var_in_mem == R->var_in_mem && L->offset == R->offset && L->var_in_reg == R->var_in_reg)
                toRemove.push_back(it);
        } else if ((*prev)->type == Backend::LIR::InstructionType::FSTORE &&
                   (*it)->type == Backend::LIR::InstructionType::FSTORE) {
            auto L = std::static_pointer_cast<Backend::LIR::StoreInt>(*prev);
            auto R = std::static_pointer_cast<Backend::LIR::StoreInt>(*it);
            if (L->var_in_mem == R->var_in_mem && L->offset == R->offset && L->var_in_reg == R->var_in_reg)
                toRemove.push_back(it);
        }
    }

    for (auto rit = toRemove.rbegin(); rit != toRemove.rend(); ++rit) {
        instructions.erase(*rit);
    }
}

void RISCV::Opt::PeepholeBeforeRA::preRAConstValueReUse(const std::shared_ptr<Backend::LIR::Block> &block) {
    const int range = 16;
    // value -> (reg, remaining_ticks)
    std::unordered_map<int32_t, std::pair<std::shared_ptr<Backend::Variable>, int>> map;
    std::vector<std::shared_ptr<Backend::LIR::Instruction>> newList;
    newList.reserve(block->instructions.size());

    auto mergeReg = [&](const std::shared_ptr<Backend::Variable> &newVar,
                        const std::shared_ptr<Backend::Variable> &oldVar) {
        for (auto &inst: block->instructions) {
            // get_used_variables 返回所有被读到的变量
            for (auto &used: inst->get_used_variables()) {
                if (used == oldVar) {
                    inst->update_used_variable(oldVar, newVar);
                }
            }
        }
    };

    for (auto &inst: block->instructions) {
        // ——1—— 如果定义了 register 且不是 load-imm，就删除所有映射到它的常量
        if (auto defVar = inst->get_defined_variable()) {
            if (inst->type != Backend::LIR::InstructionType::LOAD_IMM) {
                for (auto it = map.begin(); it != map.end();) {
                    if (it->second.first == defVar)
                        it = map.erase(it);
                    else
                        ++it;
                }
            }
        }

        // ——2—— 处理 LoadIntImm
        if (inst->type == Backend::LIR::InstructionType::LOAD_IMM) {
            auto li = std::static_pointer_cast<Backend::LIR::LoadIntImm>(inst);
            int32_t value = li->immediate->int32_value; // 常量值
            auto defReg = li->var_in_reg;

            auto found = map.find(value);
            if (found != map.end()) {
                // 命中：复用已有寄存器
                auto &entry = found->second;
                if (entry.first != defReg) {
                    mergeReg(entry.first, defReg);
                }
                entry.second = range; // 刷新生存周期
                continue; // 丢弃本条 load-imm
            } else {
                // 首次出现：删掉所有映射到同一 reg 的旧条目，再插入
                for (auto it = map.begin(); it != map.end();) {
                    if (it->second.first == defReg)
                        it = map.erase(it);
                    else
                        ++it;
                }
                map.emplace(value, std::make_pair(defReg, range));
            }
        }

        // ——3—— 非跳过的指令都加回 newList
        newList.push_back(inst);

        // ——4—— 生存周期递减，0 则移除
        for (auto it = map.begin(); it != map.end();) {
            if (--(it->second.second) == 0)
                it = map.erase(it);
            else
                ++it;
        }
    }

    // ——5—— 用 newList 重建指令流
    block->instructions = std::move(newList);
}

void RISCV::Opt::PeepholeBeforeRA::preRAConstPointerReUse(const std::shared_ptr<Backend::LIR::Block> &block) {
    const int range = 16;
    // 全局变量 → (首次装载该全局地址的虚拟寄存器, 剩余生存周期)
    std::unordered_map<std::shared_ptr<Backend::Variable>, std::pair<std::shared_ptr<Backend::Variable>, int>> map;
    std::vector<std::shared_ptr<Backend::LIR::Instruction>> newList;
    newList.reserve(block->instructions.size());

    // 合并寄存器时，需要把所有后续对 oldVar 的使用替换成 newVar
    auto mergeReg = [&](const std::shared_ptr<Backend::Variable> &newVar,
                        const std::shared_ptr<Backend::Variable> &oldVar) {
        for (auto &inst: block->instructions) {
            for (auto &used: inst->get_used_variables()) {
                if (used == oldVar) {
                    inst->update_used_variable(oldVar, newVar);
                }
            }
        }
    };

    for (auto &inst: block->instructions) {
        // ——1—— 遇到任何定义了寄存器的指令 (非加载地址)，删除所有映射到它的条目
        if (auto def = inst->get_defined_variable()) {
            if (inst->type != Backend::LIR::InstructionType::LOAD_ADDR) {
                for (auto it = map.begin(); it != map.end();) {
                    if (it->second.first == def)
                        it = map.erase(it);
                    else
                        ++it;
                }
            }
        }

        // ——2—— 如果是加载全局地址指令 ——
        if (inst->type == Backend::LIR::InstructionType::LOAD_ADDR) {
            auto la = std::static_pointer_cast<Backend::LIR::LoadAddress>(inst);
            std::shared_ptr<Backend::Variable> gv = la->var_in_mem; // 全局变量标识
            auto targetReg = la->addr; // 装载到哪个虚拟寄存器

            auto found = map.find(gv);
            if (found != map.end()) {
                // 已经装载过该全局地址：复用首次寄存器，刷新生存期，并丢弃本条 la
                auto &entry = found->second;
                if (entry.first != targetReg) {
                    mergeReg(entry.first, targetReg);
                }
                entry.second = range;
                continue;
            } else {
                // 首次遇到：删除所有映射到相同寄存器的旧条目，再插入新映射
                for (auto it = map.begin(); it != map.end();) {
                    if (it->second.first == targetReg)
                        it = map.erase(it);
                    else
                        ++it;
                }
                map.emplace(gv, std::make_pair(targetReg, range));
            }
        }

        // ——3—— 非丢弃的指令都加入 newList
        newList.push_back(inst);

        // ——4—— 生存期递减：到 0 则清理映射
        for (auto it = map.begin(); it != map.end();) {
            if (--(it->second.second) == 0)
                it = map.erase(it);
            else
                ++it;
        }
    }

    // ——5—— 重建 Block 的指令列表
    block->instructions = std::move(newList);
}

/*-------------------------------------第二轮------------------------------------------*/
// TODO:需要对齐分析
void RISCV::Opt::PeepholeBeforeRA::Lsw2Lsd(const std::shared_ptr<Backend::LIR::Block> &block) {}


RISCV::Opt::PeepholeAfterRA::PeepholeAfterRA(const std::shared_ptr<RISCV::Module> &module) { this->module = module; }

RISCV::Opt::PeepholeAfterRA::~PeepholeAfterRA() { this->module = nullptr; }

void RISCV::Opt::PeepholeAfterRA::optimize() {
    for (auto &function: module->functions) {
        for (auto &block: function->blocks) {
            addSubZeroRemove(block);
        }
        removeUselessJumps(function);
    }
}

void RISCV::Opt::PeepholeAfterRA::removeUselessJumps(const std::shared_ptr<RISCV::Function> &function) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < function->blocks.size(); i++) {
            std::shared_ptr<RISCV::Block> block = function->blocks[i];
            if (block->instructions.size() == 1) {
                if (!std::dynamic_pointer_cast<RISCV::Instructions::Jump>(block->instructions.front()))
                    continue;
                changed = true;
                std::shared_ptr<RISCV::Instructions::Jump> jump = std::static_pointer_cast<RISCV::Instructions::Jump>(block->instructions.front());
                std::shared_ptr<RISCV::Block> jump_to = jump->target_block;
                for (size_t j = 0; j < function->blocks.size(); j++)
                    for (size_t k = 0; k < function->blocks[j]->instructions.size(); k++)
                        function->blocks[j]->instructions[k]->update_block(block, jump_to);
                function->blocks.erase(function->blocks.begin() + i--);
            }
        }
    }
}

void RISCV::Opt::PeepholeAfterRA::addSubZeroRemove(const std::shared_ptr<RISCV::Block> &block) {
    auto &instructions = block->instructions;
    auto it = instructions.begin();
    while (it != instructions.end()) {
        bool erased = false;
        // 动态识别 Sub（Rtype）
        if (auto sub = std::dynamic_pointer_cast<RISCV::Instructions::Sub>(*it)) {
            // rd == rs1 且 rs2 是零寄存器 x0
            if (sub->rd != Registers::ABI::ZERO && (sub->rd == sub->rs1 && sub->rs2 == Registers::ABI::ZERO)) {
                it = instructions.erase(it);
                erased = true;
            }
        } else if (auto add = std::dynamic_pointer_cast<RISCV::Instructions::Add>(*it)) {
            // rd == rs1 且 rs2 是零寄存器 x0 || rd == rs2 且 rs1 是零寄存器 x0
            if (add->rd != Registers::ABI::ZERO && ((add->rd == add->rs1 && add->rs2 == Registers::ABI::ZERO) ||
                                                    (add->rd == add->rs2 && add->rs1 == Registers::ABI::ZERO))) {
                it = instructions.erase(it);
                erased = true;
            }
        } else if (auto subImm = std::dynamic_pointer_cast<RISCV::Instructions::SubImmediate>(*it)) {
            if (subImm->rd != Registers::ABI::ZERO && ((subImm->rd == subImm->rs1 && subImm->imm == 0))) {
                it = instructions.erase(it);
                erased = true;
            }
        } else if (auto addImm = std::dynamic_pointer_cast<RISCV::Instructions::AddImmediate>(*it)) {
            if (addImm->rd != Registers::ABI::ZERO && ((addImm->rd == addImm->rs1 && addImm->imm == 0))) {
                it = instructions.erase(it);
                erased = true;
            }
        }
        if (!erased) {
            ++it;
        }
    }
}
