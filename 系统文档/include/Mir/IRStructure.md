# IRStructure.h

## IRModule

+ IRGlobalVariable (向量)
+ IRFunction （向量）



## InitValue

+ 辅助结构，用于记录初始化内容
+ 内容
  + int / int array
  + float / float array

## IRGlobalVariable

+ 标识符（String）
+ 类型（IRType）
+ 初始化元素列（InitValue）



##  IRFunction

+ 标识符 （String）
+ 返回值类型 （IRType）
+ 参数列表 (fParam 向量)
+ 基本块列表 (IRBlock 向量)



## IRFParam

+ 类型 （IRType）

  

## IRBlock

+ blockID （int）
+ IRInstruction (向量)