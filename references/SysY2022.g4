grammar SysY2022;

// 主规则
compUnit: (decl | funcDef)* EOF;

// 声明部分
decl: constDecl | varDecl;
constDecl: 'const' bType=('int' | 'float') constDef[$bType.text] (',' constDef[$bType.text])* ';';
varDecl: bType=('int' | 'float') varDef[$bType.text] (',' varDef[$bType.text])* ';';

// 常量定义
constDef[String type]: IDENT ('[' constExp ']')* '=' constInitVal;
constInitVal: constExp | '{' (constInitVal (',' constInitVal)*)? '}';

// 变量定义
varDef[String type]: IDENT ('[' constExp ']')* ('=' initVal)?;
initVal: exp | '{' (initVal (',' initVal)*)? '}';

// 函数定义
funcDef: funcType=('void' | 'int' | 'float') IDENT '(' funcFParams? ')' block;
funcFParams: funcFParam (',' funcFParam)*;
funcFParam: bType=('int' | 'float') IDENT ('[' ']' ('[' exp ']')*)?;

// 语句块和语句
block: '{' blockItem* '}';
blockItem: decl | stmt;
stmt: lVal '=' exp ';'                            # assignStmt
    | 'putf' '(' StringConst (',' exp)* ')' ';'   # putfStmt
    | exp? ';'                                    # expStmt
    | block                                       # blockStmt
    | 'if' '(' cond ')' stmt ('else' stmt)?       # ifStmt
    | 'while' '(' cond ')' stmt                   # whileStmt
    | 'break' ';'                                 # breakStmt
    | 'continue' ';'                              # continueStmt
    | 'return' exp? ';'                           # returnStmt
    ;

// 表达式部分
exp: addExp;
cond: lOrExp;
lVal: IDENT ('[' exp ']')*;
primaryExp: '(' exp ')' | lVal | number;
number: FLOAT_NUMBER | HEX_CONST | OCT_CONST | INT_CONST;
unaryExp: primaryExp | IDENT '(' funcRParams? ')' | unaryOp unaryExp;
unaryOp: ADD | SUB | NOT;
funcRParams: exp (',' exp)*;
mulExp: unaryExp (op=(MUL | DIV | MOD) unaryExp)*;
addExp: mulExp (op=(ADD | SUB) mulExp)*;
relExp: addExp (op=(LE | GE | LT | GT) addExp)*;
eqExp: relExp (op=(EQ | NE) relExp)*;
lAndExp: eqExp (AND eqExp)*;
lOrExp: lAndExp (OR lAndExp)*;
constExp: addExp;

// 词法定义
IDENT: [a-zA-Z_][a-zA-Z_0-9]*;
ADD: '+';
SUB: '-';
NOT: '!';
MUL: '*';
DIV: '/';
MOD: '%';
LT: '<';
GT: '>';
LE: '<=';
GE: '>=';
EQ: '==';
NE: '!=';
AND: '&&';
OR: '||';

// 支持十进制、十六进制和八进制整数
INT_CONST: '0' | [1-9][0-9]*;
HEX_CONST: '0x' [0-9a-fA-F]+ | '0X' [0-9a-fA-F]+;
OCT_CONST: '0' [0-7]+;

// 浮点数定义
FLOAT_NUMBER
    : DecimalFloatingConstant
    | HexadecimalFloatingConstant
    ;

// 十进制浮点数
fragment DecimalFloatingConstant
    : FractionalConstant ExponentPart? FloatingSuffix?
    | DigitSequence ExponentPart FloatingSuffix?
    ;

// 十六进制浮点数
fragment HexadecimalFloatingConstant
    : HexadecimalPrefix HexadecimalFractionalConstant BinaryExponentPart FloatingSuffix?
    | HexadecimalPrefix HexadecimalDigitSequence BinaryExponentPart FloatingSuffix?
    ;

// 分数常量
fragment FractionalConstant
    : DigitSequence? '.' DigitSequence
    | DigitSequence '.'
    ;

// 十进制指数部分
fragment ExponentPart
    : [eE] Sign? DigitSequence
    ;

// 二进制指数部分
fragment BinaryExponentPart
    : [pP] Sign? DigitSequence
    ;

// 符号
fragment Sign
    : '+' | '-'
    ;

// 十进制数字序列
fragment DigitSequence
    : Digit+
    ;

// 十六进制分数常量
fragment HexadecimalFractionalConstant
    : HexadecimalDigitSequence? '.'
    | HexadecimalDigitSequence '.'
    ;

// 十六进制数字序列
fragment HexadecimalDigitSequence
    : HexadecimalDigit+
    ;

// 十六进制前缀
fragment HexadecimalPrefix
    : '0' [xX]
    ;

// 浮点数后缀
fragment FloatingSuffix
    : [fF] | [lL]
    ;

// 数字
fragment Digit
    : [0-9]
    ;

// 十六进制数字
fragment HexadecimalDigit
    : [0-9a-fA-F]
    ;

// 字符串常量
StringConst: '"' (~["\\] | '\\' .)* '"' ;

// 空白符和注释
WS: [ \t\r\n]+ -> skip;
LINE_COMMENT: '//' ~[\r\n]* -> skip;
BLOCK_COMMENT: '/*' .*? '*/' -> skip;
