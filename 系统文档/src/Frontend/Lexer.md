# Lexer.cpp

+ 引用头文件 Frontend/Lexer.h

## consume_line_comment()

​	匹配掉两个 / 字符，之后一直匹配下一个\n前的所有字符

​	其中做越界检查，防止最后一行之后无\n

## consume_block_comment()

​	匹配掉/*字符，之后一直匹配知道遇到\*/ 再将其匹配掉

​	其中做越界检查

## consume_ident_or_keyword()

​	以首字符所在行数为 token 行数，用alnum _ 判断是否是标识符中字符。

​	之后以 keywords.find 检查字符内容，找到时返回对应关键字 token 否则返回 lexer Token.

## consume_number

感觉是 GPT 生成的程序

识别前缀，并按前缀分进制识别数字，区分整数与浮点数

这里直接转换为十进制整数或浮点数，再转换为字符串

+ GPT 告诉我直接 stod 不支持转十六进制/八进制浮点数，但实测可以

## consume_string

首先吸收一个引号，之后若为转义符号，连续吸收两个字符；若为引号则结束，否则继续

字符串形成 Token 时，字符内容只包含双引号中间内容，不包含双引号内容

## consume_operator

先匹配两个字符，匹配不成功：匹配单个字符，否则返回特殊符号 UNKNOWN

## tokenize

检查首字符，观察是否需要跳过空白部分或注释

之后分为四种情况：

+ 字母/_ ：标识符
+ 数字：num
+ 双引号：字符串
+ 其他：分隔符或特殊字符

最后加入特殊的 END_OF_FILE TOKEN