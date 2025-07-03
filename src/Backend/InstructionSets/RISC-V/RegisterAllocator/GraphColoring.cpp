// void RegisterAllocator::build_interference_graph() {
//     // 遍历所有基本块
//     for (const auto& [block_name, block] : function_->blocks) {
//         // 对于每个出现在live_out中的变量对
//         for (const auto& var1 : block->live_out) {
//             for (const auto& var2 : block->live_out) {
//                 if (var1 != var2) {
//                     // 添加两个变量之间的干涉边
//                     interference_graph_[var1->name].insert(var2->name);
//                     interference_graph_[var2->name].insert(var1->name);
//                 }
//             }
//         }
        
//         // 处理区块内的变量干涉
//         std::unordered_set<std::string> live_vars;
//         for (const auto& var : block->live_out) {
//             live_vars.insert(var->name);
//         }
        
//         // 从后向前遍历指令
//         for (auto it = block->instructions.rbegin(); it != block->instructions.rend(); ++it) {
//             auto instr = *it;
            
//             // 处理定义的变量
//             auto defined = instr->get_defined_variable();
//             if (defined) {
//                 // 定义的变量与所有当前活跃的变量冲突
//                 for (const auto& live_var : live_vars) {
//                     if (defined->name != live_var) {
//                         interference_graph_[defined->name].insert(live_var);
//                         interference_graph_[live_var].insert(defined->name);
//                     }
//                 }
                
//                 // 定义后变量不再活跃
//                 live_vars.erase(defined->name);
//             }
            
//             // 处理使用的变量
//             auto used_vars = instr->get_used_variables();
//             for (const auto& used : *used_vars) {
//                 // 使用的变量变为活跃
//                 live_vars.insert(used->name);
//             }
//         }
//     }
// }

// void RegisterAllocator::graph_coloring_allocation() {
//     build_interference_graph();
    
//     // 构建节点栈，按照度数排序
//     std::vector<std::string> stack;
//     std::unordered_set<std::string> remaining_nodes;
    
//     // 初始化剩余节点集合
//     for (const auto& [var_name, _] : interference_graph_) {
//         remaining_nodes.insert(var_name);
//     }
    
//     // 简化图直到没有剩余节点
//     while (!remaining_nodes.empty()) {
//         // 找到度数最小的节点
//         std::string min_degree_node;
//         size_t min_degree = std::numeric_limits<size_t>::max();
        
//         for (const auto& node : remaining_nodes) {
//             size_t degree = 0;
//             for (const auto& neighbor : interference_graph_[node]) {
//                 if (remaining_nodes.find(neighbor) != remaining_nodes.end()) {
//                     degree++;
//                 }
//             }
            
//             if (degree < min_degree) {
//                 min_degree = degree;
//                 min_degree_node = node;
//             }
//         }
        
//         // 从图中删除该节点并加入栈
//         stack.push_back(min_degree_node);
//         remaining_nodes.erase(min_degree_node);
//     }
    
//     // 着色阶段
//     int stack_offset = 0;
//     std::unordered_set<RISCV::Registers::ABI> used_colors;
    
//     // 从栈中弹出节点并为其分配颜色
//     while (!stack.empty()) {
//         std::string node = stack.back();
//         stack.pop_back();
        
//         // 找出邻居已经使用的颜色
//         used_colors.clear();
//         for (const auto& neighbor : interference_graph_[node]) {
//             if (var_to_reg_.find(neighbor) != var_to_reg_.end()) {
//                 used_colors.insert(var_to_reg_[neighbor]);
//             }
//         }
        
//         // 尝试找一个可用的颜色(寄存器)
//         bool color_found = false;
//         for (const auto& reg : available_regs_) {
//             if (used_colors.find(reg) == used_colors.end()) {
//                 // 找到可用颜色
//                 var_to_reg_[node] = reg;
//                 color_found = true;
//                 break;
//             }
//         }
        
//         // 如果没有可用颜色，需要溢出到栈
//         if (!color_found) {
//             var_to_stack_[node] = stack_offset;
//             stack_offset += 8; // 假设每个变量占用8字节
//         }
//     }
    
//     // 更新函数需要的栈空间大小
//     stack_size_ = stack_offset;
// }