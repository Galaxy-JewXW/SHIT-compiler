# SHIT-Compiler
SysY 语言编译器，计算机系统能力大赛编译系统设计赛（华为毕昇杯）参赛作品。

## 构建
```bash
# 在根目录下执行
mkdir build && cd build
cmake .. -D CMAKE_BUILD_TYPE={Debug, Release, RelWithDebInfo} # 设定优化等级
make -j 4
# 构建好的目标为../bin/compiler
```

在Debug模式下，定义了宏 `-DSHIT_DEBUG` ，设置日志输出等级为 `TRACE`；默认情况下为 `INFO`。

## 命令行使用方法

```bash
./compiler 输入文件 [选项]
```

Debug模式下，会载入默认数据，方便开发：
```c++
compiler_options debug_compile_options = {
    .input_file = "../testcase.sy",
    .flag_S = true,
    .output_file = "../testcase.s",
    ._emit_options = {.emit_tokens = false, .emit_ast = false, .emit_llvm = true},
    .opt_level = Optimize_level::O0
};
```

### 必需参数

- `输入文件`：源代码文件路径（使用 .sy 扩展名）

### 可选参数

#### 输出控制
- `-S`：生成汇编代码输出
- `-o <文件>`：指定汇编输出文件（默认为：输入文件名.s）

#### 优化选项
- `-O0`：不进行优化（默认选项）
- `-O1`：基础优化
- `-O2`：高级优化

#### 中间表示输出
- `-emit-tokens [<文件>]`：输出词法标记
- `-emit-ast [<文件>]`：输出抽象语法树
- `-emit-llvm [<文件>]`：输出 LLVM IR（默认为：输入文件名.ll）

对于所有的 emit 选项，如果不指定输出文件，将直接输出到标准输出（stdout）。

## 使用示例

1. 基本的汇编代码生成：
```bash
./compiler test.sy -S
```
这将生成 `test.s` 作为输出文件。

2. 指定自定义输出文件并启用优化：
```bash
./compiler source.sy -S -o output.s -O2
```

3. 输出所有中间表示：
```bash
./compiler program.sy -emit-tokens tokens.txt -emit-ast ast.txt -emit-llvm ir.ll
```

4. 使用优化并查看词法标记：
```bash
./compiler input.sy -O1 -emit-tokens
```
词法标记将直接打印到标准输出。

5. 同时生成汇编代码和 LLVM IR：
```bash
./compiler test.sy -S -emit-llvm
```
将创建 `test.s` 和 `test.ll` 两个文件。

## 输出示例

运行编译器时，会显示解析后的选项信息。例如：
```
[    0ms] INFO  Driver.cpp:41: Options: -input=../testcase.sy, -S=false, opt=-O1, -emit-ast=stdout, -emit-llvm=../testcase.ll
```

## 错误提示

编译器在以下情况会显示相应的错误信息：
- 未指定输入文件
- 指定了多个输入文件
- 使用了未知的命令行选项
- 无法打开输入/输出文件
- 缺少必需的选项参数

## 优化思路
### 中端优化
#### 前置分析
- [ ] **别名分析**（Alias Analysis）
  - 流敏感与流不敏感分析
  - 基于类型的别名推断
  - 支持指针逃逸分析
- [ ] **内存依赖分析**（Memory Dependency Analysis）
  - 检测Load-Store/Store-Store依赖关系
  - 构建内存访问依赖图（MADG）
- [ ] **基指针分析**（Base Pointer Analysis）
  - 识别数组/结构体的基地址
  - 用于循环优化和冗余访存消除

#### 控制流优化
- [ ] **SimplifyCFG**
  - 合并相邻基本块
  - 消除空跳转块（如仅含`br %l`的基本块）
  - 三角变换优化（If-conversion）
  - 开关语句降级为跳转表
- [ ] **RemoveBlocks**
  - 删除不可达基本块
  - 合并单前驱单后继块
- [ ] **分支预测**（Branch Prediction）
  - 静态分支概率分析
  - 热路径块排序标记（为后端布局提供信息）

#### 数据流优化
- [ ] **Mem2Reg**
  - 提升Alloca到SSA寄存器
  - 消除冗余栈操作
- [ ] **GVN & GCM**
  - 全局值编号消除冗余计算
  - 代码提升（Hoisting）与下沉（Sinking）
  - 支配深度优化调度
- [ ] **LVN**
  - 局部值编号优化
  - 基于支配树的表达式替换

#### 循环优化
- [ ] **LoopSimplifyForm**
  - 规范化循环结构：
    - 唯一preheader生成
    - 唯一latch块确认
    - Exit块支配关系验证
- [ ] **LCSSA**（Loop-Closed SSA）
  - 在exit块插入φ节点隔离循环变量
- [ ] **LoopUnroll**
  - 全展开（ConstLoopUnroll）：迭代次数确定的小循环
  - 部分展开（4路展开）：动态循环次数
  - 展开后冗余PHI节点消除
- [ ] **LoopInvariantCodeMotion**
  - 循环不变量外提
  - 结合SCEV分析优化归纳变量
- [ ] **LoopUnswitching**
  - 提升循环内条件判断到外层
  - 创建循环克隆版本优化路径

#### 内存优化
- [ ] **ConstIdx2Value**
  - 常量下标数组访问内联化（如`a[5]`→`%v`）
- [ ] **LocalArrayLift**
  - 局部数组全局化条件分析：
    - 生命周期验证
    - 无指针逃逸保证
- [ ] **SoraPass**
  - 数组标量化拆分策略：
    - 按访问模式分解（行优先/列优先）
    - 最大标量元素阈值控制
- [ ] **RedundantAccessElimination**
  - 存储消除：消除dead store
  - 加载合并：相同地址连续load合并
  - 寄存器缓存局部变量

#### 函数优化
- [ ] **DeadArgEliminate**
  - 无用参数删除
  - 调用点参数同步更新
- [ ] **FuncInline**
  - 选择性内联策略：
    - 小函数阈值（<10指令）
    - 高频调用热点标记
    - 递归内联深度控制
- [ ] **TailCall2Loop**
  - 尾递归模式识别
  - 栈帧复用验证
  - 循环结构生成

#### 算术优化
- [ ] **StrengthReduction**
  - 乘法→移位加法（如`x*7 → (x<<3)-x`）
  - 除法→乘法逆元（Magic Number优化）
- [ ] **Reassociation**
  - 表达式重排优化结合律
  - 常量折叠子表达式
- [ ] **RangeAnalysis**
  - 值域传播（如数组下标非负验证）
  - 符号推断（Signed/Unsigned）

### 后端优化
#### 寄存器分配
- [ ] **WeightedRegisterAllocation**
  - 权重模型：
    - 循环嵌套深度系数
    - 执行频率预估
    - 生存区间跨度
  - 溢出代价敏感着色算法

#### 指令调度
- [ ] **BlockLayout**
  - 热路径连续布局
  - Fall-through优化：
    - 省略冗余跳转指令
    - 分支目标对齐
- [ ] **PeepholeOptimization**
  - 指令序列模式匹配优化：
    - `srai+slli` → `andi`
    - 连续立即数加载合并
  - 冗余移动消除（如`mv a0,a0`）

#### 内存访问优化
- [ ] **LoadStoreForwarding**
  - 局部存储队列跟踪
  - 寄存器缓存最近访问地址
- [ ] **AddressReuse**
  - 基址寄存器复用分析
  - 偏移量合并优化（如`lw a0,0(sp); lw a1,4(sp)`→双字加载）

#### 乘除优化
- [ ] **ConstantMultiplication**
  - Booth算法优化选择
  - 移位加法分解策略
  - 代价模型（指令数 vs 周期数）
- [ ] **DivisionOptimization**
  - 按论文《Division by Invariant Integers using Multiplication》实现
  - 2^n除法→移位优化
  - 非精确除法余数补偿

#### 代码生成增强
- [ ] **BlockInline**
  - 直接跳转块内联条件：
    - 单前驱单后继
    - 指令数阈值控制
- [ ] **JumpThreading**
  - 条件跳转链简化
  - 间接跳转降级为直接


### 优化流水线设计建议
1. **阶段顺序**：
   ```
   Mem2Reg → LoopSimplify → LCSSA → GVN → GCM → DCE → LoopUnroll
   ```
2. **迭代优化**：
  - 在主要优化后加入轻量级DCE
  - 关键pass设置多次运行（如GVN+SimplifyCFG循环）
3. **Profile-Guided优化**：
  - 插入插桩代码收集分支概率
  - 热块标记与冷块分离
4. **Debug支持**：
  - 各优化阶段可配置开关
  - 优化前后CFG/SSA验证
5. **架构扩展**：
  - SIMD指令模式识别
  - 自动向量化基础支持