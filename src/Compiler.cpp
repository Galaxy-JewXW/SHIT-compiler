#include "Compiler.h"

#include <fstream>
#include <iostream>
#include <sstream>

bool options[28];

void parseArgs(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--print-token") {
            options['t' - 'a'] = true;
            std::cout << "Printing tokens." << std::endl;
        } else if (arg == "--print-ast") {
            options['a' - 'a'] = true;
            std::cout << "Printing AST." << std::endl;
        } else if (arg == "--print-ir") {
            options['i' - 'a'] = true;
            std::cout << "Printing IR." << std::endl;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            exit(1);
        }
    }
}

int main(int argc, char *argv[]) {
    parseArgs(argc, argv);
    // 先指定了源文件路径
    const std::string filename = "../testfile.sy";
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << "\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string sourceCode = buffer.str();
    file.close();

    Lexer lexer(sourceCode);
    std::vector<Token::Token> tokens = lexer.tokenize();

    // 输出Token
    for (const auto &token: tokens) {
        token.print();
    }

    return 0;
}
