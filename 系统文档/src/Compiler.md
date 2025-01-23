# Compiler.cpp

+ 引入头文件 Compiler.h

+ 全局变量数组 `bool options[20]` 代表参数



## main

解析参数

指定文件名为`../testfile.sy`,打开获取输入内容到缓冲区后关闭文件。

+ 做文件未打开的异常检查

字符流传入 lexer 获得 Token vertor : tokens

tokens 传入 parser 获得 ast

调用 ast->to_string() (std::string) 并输出至cout 



## parseArgs

```
void parseArgs(int argc, char *argv[])
```

根据参数个数与序列，为选项数组赋值并输出信息

+ --print-token 对应输出 token 序列
  + option['t' - 'a'] 置 true

+ --print-ast 对应输出语法树
  + option['a' - 'a'] 置 true
+ --print-it 对应输出 IR
  + options['i'-'a'] 置 true

有其余参数，则输出异常信息（异常参数）并退出执行