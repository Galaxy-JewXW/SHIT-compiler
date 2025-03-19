# SHIT-Compiler README文档
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