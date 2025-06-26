#include "Backend/InstructionSets/RISC-V/RegisterAllocator.h"
// #include "Backend/InstructionSets/RISC-V/Modules.h"
#include "Utils/Log.h"
#include <algorithm>
#include <queue>
#include <stack>

std::unique_ptr<RISCV::RegisterAllocator::Base> RISCV::RegisterAllocator::Base::create(AllocStrategy strategy) {
    switch (strategy) {
        case RISCV::RegisterAllocator::AllocStrategy::LINEAR:
            return std::make_unique<RISCV::RegisterAllocator::LinearAllocator>();
        // case RISCV::RegisterAllocator::AllocStrategy::GRAPH_COLOR:
        //     return std::make_unique<RISCV::RegisterAllocator::GraphColoringAllocator>();
        default:
            log_warn("Unrealized allocation strategy, defaulting to LINEAR");
            return std::make_unique<RISCV::RegisterAllocator::LinearAllocator>();
    }
}

// void RISCV::RegisterAllocator::LinearAllocator::analyse(RISCV::Modules::Function& function) {
//     log_debug("Alloc for function: %s", function->get_name().c_str());
//     RISCV::Registers::clear();
//     int pcount = 0;
//     for (const std::shared_ptr<Mir::Argument>& parameter : function->get_arguments()) {
//         std::shared_ptr<Backend::MIR::GlobalVariable> param = std::make_shared<Backend::MIR::GlobalVariable>(parameter->get_name(), Backend::Utils::llvm_to_riscv(*parameter->get_type()));
//         size_t size = Backend::Utils::type_to_size(param->type);
//         param->offset = function_field.sp->offset += size;
//         log_debug("Alloc: %s, size: %zu, $sp: %zu", param->name.c_str(), size, function_field.sp->offset);
//         if (pcount < 3) {
//             RISCV::Registers::alloc(RISCV::Registers::ABI::A0 + pcount, param);
//             pcount++;
//         } else param->reg = RISCV::Registers::ABI::STACK;
//         function_field.add_variable(param);
//     }
//     for (const std::shared_ptr<Mir::Block>& block : function->get_blocks()) {
//         for (const std::shared_ptr<Mir::Instruction>& instruction : block->get_instructions()) {
//             if (instruction->get_op() == Mir::Operator::ALLOC) {
//                 std::shared_ptr<Mir::Alloc> alloc = std::dynamic_pointer_cast<Mir::Alloc>(instruction);
//                 Backend::MIR::GlobalVariable var(alloc->get_name(), Backend::Utils::llvm_to_riscv(*alloc->get_type()));
//                 size_t size = Backend::Utils::type_to_size(var.type);
//                 var.offset = function_field.sp->offset += size;
//                 log_debug("Alloc: %s, size: %zu, $sp: %zu", var.name.c_str(), size, function_field.sp->offset);
//                 var.reg = RISCV::Registers::ABI::STACK;
//                 function_field.add_variable(std::make_shared<Backend::MIR::GlobalVariable>(var));
//             } else {
//                 std::vector<std::shared_ptr<Backend::MIR::Variable>> vars;
//                 if (std::string vreg = instruction->get_name(); !vreg.empty()) {
//                     Backend::MIR::GlobalVariable var(vreg, Backend::Utils::llvm_to_riscv(*instruction->get_type()));
//                     size_t size = Backend::Utils::type_to_size(var.type);
//                     var.offset = function_field.sp->offset += size;
//                     log_debug("Alloc: %s, size: %zu, $sp: %zu", var.name.c_str(), size, function_field.sp->offset);
//                     std::shared_ptr<Backend::MIR::Variable> ptr = std::make_shared<Backend::MIR::Variable>(var);
//                     function_field.add_variable(ptr);
//                     vars.push_back(ptr);
//                 }
//                 for (const std::shared_ptr<Mir::Value> &operand : instruction->get_operands()) {
//                     if (std::shared_ptr<Backend::MIR::Variable> vvar = function_field.variables[operand->get_name()]) {
//                         vars.push_back(vvar);
//                     }
//                 }
//             }
//         }
//     }
//     function_field.sp->alloc_record.push_back(function_field.sp->offset);
// }

// void RISCV::RegisterAllocator::GraphColoringAllocator::allocate(const std::shared_ptr<Mir::Function>& function, RISCV::Modules::FunctionField& function_field) {
//     log_debug("Graph coloring register allocation for function: %s", function->get_name().c_str());

//     std::vector<LiveInterval> liveIntervals = computeLiveIntervals(function);
//     auto interferenceGraph = buildInterferenceGraph(liveIntervals);
//     auto coloring = colorGraph(interferenceGraph);

//     std::vector<RISCV::Registers::ABI> availableRegs = {
//         RISCV::Registers::ABI::T0, RISCV::Registers::ABI::T1, RISCV::Registers::ABI::T2,
//         RISCV::Registers::ABI::T3, RISCV::Registers::ABI::T4, RISCV::Registers::ABI::T5,
//         RISCV::Registers::ABI::T6,
//         RISCV::Registers::ABI::S2, RISCV::Registers::ABI::S3, RISCV::Registers::ABI::S4,
//         RISCV::Registers::ABI::S5, RISCV::Registers::ABI::S6, RISCV::Registers::ABI::S7,
//         RISCV::Registers::ABI::S8, RISCV::Registers::ABI::S9, RISCV::Registers::ABI::S10,
//         RISCV::Registers::ABI::S11
//     };

//     // 处理函数参数
//     for (const std::shared_ptr<Mir::Argument>& parameter : function->get_arguments()) {
//         std::string vreg = parameter->get_name();
//         std::shared_ptr<Mir::Type::Type> type_ = parameter->get_type();
//         size_t size = Backend::Utils::type_to_size(Backend::Utils::llvm_to_riscv(*type_));

//         auto it = coloring.find(vreg);
//         if (it != coloring.end()) {
//             // 参数被分配了寄存器，记录映射关系
//             log_debug("Parameter %s assigned to register %s", vreg.c_str(), RISCV::Registers::reg2string(it->second).c_str());
//             // 此处需要创建寄存器到变量的映射，供后续指令生成使用
//         } else {
//             // 参数没有被分配寄存器，使用栈空间
//             // function_field.memory->vreg2offset[vreg] = function_field.sp->offset += size;
//             log_debug("Parameter %s spilled to stack, offset: %zu", 
//                     vreg.c_str(), function_field.sp->offset);
//         }
//     }
    
//     // 处理所有指令
//     for (const std::shared_ptr<Mir::Block>& block : function->get_blocks()) {
//         for (const std::shared_ptr<Mir::Instruction>& instruction : block->get_instructions()) {
//             if (instruction->get_op() == Mir::Operator::ALLOC) {
//                 std::shared_ptr<Mir::Alloc> alloc = std::dynamic_pointer_cast<Mir::Alloc>(instruction);
//                 std::string vreg = alloc->get_name();
//                 std::shared_ptr<Mir::Type::Type> type_ = alloc->get_type();
//                 size_t size = Backend::Utils::type_to_size(
//                     Backend::Utils::llvm_to_riscv(*type_));
                
//                 // alloc总是分配栈空间
//                 // function_field.memory->vptr2offset[vreg] = function_field.sp->offset += size;
//                 log_debug("Alloc: %s, size: %zu, $sp: %zu", vreg.c_str(), size, function_field.sp->offset);
//             } else if (std::string vreg = instruction->get_name(); !vreg.empty()) {
//                 std::shared_ptr<Mir::Type::Type> type_ = instruction->get_type();
//                 size_t size = Backend::Utils::type_to_size(
//                     Backend::Utils::llvm_to_riscv(*type_));
                
//                 // 检查变量是否被分配了寄存器
//                 auto it = coloring.find(vreg);
//                 if (it != coloring.end()) {
//                     // 变量被分配了寄存器
//                     log_debug("Variable %s assigned to register %s", 
//                             vreg.c_str(), RISCV::Registers::reg2string(it->second).c_str());
//                     // 此处需要创建寄存器到变量的映射，供后续指令生成使用
//                 } else {
//                     // 变量没有被分配寄存器，使用栈空间
//                     // function_field.memory->vreg2offset[vreg] = function_field.sp->offset += size;
//                     log_debug("Variable %s spilled to stack, offset: %zu", 
//                             vreg.c_str(), function_field.sp->offset);
//                 }
//             }
//         }
//     }
    
//     function_field.sp->alloc_record.push_back(function_field.sp->offset);
// }

// std::vector<RISCV::RegisterAllocator::GraphColoringAllocator::LiveInterval> RISCV::RegisterAllocator::GraphColoringAllocator::computeLiveIntervals(
//     const std::shared_ptr<Mir::Function>& function) {
    
//     std::vector<LiveInterval> intervals;
//     std::map<std::string, LiveInterval> varIntervals;
    
//     // 遍历所有基本块和指令
//     int instrIndex = 0;
//     for (const std::shared_ptr<Mir::Block>& block : function->get_blocks()) {
//         for (const std::shared_ptr<Mir::Instruction>& instr : block->get_instructions()) {
//             // 处理这条指令定义的变量
//             std::string defVar = instr->get_name();
//             if (!defVar.empty()) {
//                 // 如果变量首次出现，创建新的区间
//                 if (varIntervals.find(defVar) == varIntervals.end()) {
//                     varIntervals[defVar] = {defVar, instrIndex, instrIndex};
//                 }
//                 // 更新变量的开始位置
//                 varIntervals[defVar].start = instrIndex;
//             }
            
//             // 处理这条指令使用的变量
//             for (const std::shared_ptr<Mir::Value>& operand : instr->get_operands()) {
//                 if (!operand->is_constant()) {
//                     std::string useVar = operand->get_name();
//                     if (!useVar.empty()) {
//                         // 如果变量首次出现，创建新的区间
//                         if (varIntervals.find(useVar) == varIntervals.end()) {
//                             varIntervals[useVar] = {useVar, instrIndex, instrIndex};
//                         }
//                         // 更新变量的结束位置
//                         varIntervals[useVar].end = instrIndex;
//                     }
//                 }
//             }
            
//             instrIndex++;
//         }
//     }
    
//     // 转换为vector
//     for (const auto& pair : varIntervals) {
//         intervals.push_back(pair.second);
//     }
    
//     return intervals;
// }

// // 构建冲突图
// std::map<std::string, std::set<std::string>> RISCV::RegisterAllocator::GraphColoringAllocator::buildInterferenceGraph(
//     const std::vector<LiveInterval>& intervals) {
    
//     std::map<std::string, std::set<std::string>> graph;
    
//     // 初始化图的节点
//     for (const auto& interval : intervals) {
//         graph[interval.var] = {};
//     }
    
//     // 构建图的边
//     for (size_t i = 0; i < intervals.size(); ++i) {
//         for (size_t j = i + 1; j < intervals.size(); ++j) {
//             const auto& interval1 = intervals[i];
//             const auto& interval2 = intervals[j];
            
//             // 如果两个区间重叠，则添加边
//             if (!(interval1.end < interval2.start || interval2.end < interval1.start)) {
//                 graph[interval1.var].insert(interval2.var);
//                 graph[interval2.var].insert(interval1.var);
//             }
//         }
//     }
    
//     return graph;
// }

// // 图着色算法
// std::map<std::string, RISCV::Registers::ABI> RISCV::RegisterAllocator::GraphColoringAllocator::colorGraph(
//     const std::map<std::string, std::set<std::string>>& graph) {
    
//     // 可用的物理寄存器（排除特殊寄存器如zero, ra, sp等）
//     std::vector<RISCV::Registers::ABI> availableColors = {
//         RISCV::Registers::ABI::T0, RISCV::Registers::ABI::T1, RISCV::Registers::ABI::T2,
//         RISCV::Registers::ABI::T3, RISCV::Registers::ABI::T4, RISCV::Registers::ABI::T5,
//         RISCV::Registers::ABI::T6,
//         RISCV::Registers::ABI::S2, RISCV::Registers::ABI::S3, RISCV::Registers::ABI::S4,
//         RISCV::Registers::ABI::S5, RISCV::Registers::ABI::S6, RISCV::Registers::ABI::S7,
//         RISCV::Registers::ABI::S8, RISCV::Registers::ABI::S9, RISCV::Registers::ABI::S10,
//         RISCV::Registers::ABI::S11
//     };
    
//     std::map<std::string, RISCV::Registers::ABI> coloring;
    
//     // 使用Kempe算法进行图着色
//     // 1. 计算每个节点的度数
//     std::map<std::string, int> degrees;
//     for (const auto& [node, neighbors] : graph) {
//         degrees[node] = neighbors.size();
//     }
    
//     // 2. 按度数从小到大对节点进行排序
//     std::vector<std::string> nodes;
//     for (const auto& [node, _] : graph) {
//         nodes.push_back(node);
//     }
    
//     std::sort(nodes.begin(), nodes.end(), [&degrees](const std::string& a, const std::string& b) {
//         return degrees[a] < degrees[b];
//     });
    
//     // 3. 为每个节点分配颜色
//     for (const auto& node : nodes) {
//         // 记录已经被邻居使用的颜色
//         std::set<RISCV::Registers::ABI> usedColors;
//         for (const auto& neighbor : graph.at(node)) {
//             if (coloring.find(neighbor) != coloring.end()) {
//                 usedColors.insert(coloring[neighbor]);
//             }
//         }
        
//         // 找到第一个可用的颜色
//         bool colorAssigned = false;
//         for (const auto& color : availableColors) {
//             if (usedColors.find(color) == usedColors.end()) {
//                 coloring[node] = color;
//                 colorAssigned = true;
//                 break;
//             }
//         }
        
//         // 如果没有可用颜色，不对这个节点分配寄存器（后续会溢出到栈）
//         if (!colorAssigned) {
//             log_debug("Node %s could not be assigned a register (spill)", node.c_str());
//         }
//     }
    
//     return coloring;
// }
