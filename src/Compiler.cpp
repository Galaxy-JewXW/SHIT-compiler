#include "Compiler.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include "Mir/Builder.h"

bool options[28];

void parseArgs(int argc, char *argv[]) {
#ifdef LOG_LEVEL_TRACE
    log_set_level(LOG_TRACE);
#elif defined(LOG_LEVEL_INFO)
    log_set_level(LOG_INFO);
#else
    log_set_level(LOG_DEBUG);
#endif
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
    log_info("Reading file: %s", filename.c_str());
    std::ifstream file(filename);
    if (!file.is_open()) {
        log_fatal("Could not open file: %s", filename.c_str());
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string src_code = buffer.str();
    file.close();
    log_trace("Tokenizing source code.");
    Lexer lexer(src_code);
    std::vector<Token::Token> tokens = lexer.tokenize();
    log_trace("Parsing tokens to AST.");
    Parser parser(tokens);
    std::shared_ptr<AST::CompUnit> ast = parser.parse();
    log_debug("AST info as follows: \n%s", ast->to_string().c_str());
    log_info("Building LLVM IR.");
    Mir::Builder builder;
    std::shared_ptr<Mir::Module> module = builder.visit(ast);
    log_debug("Module info as follows: \n%s", module->to_string().c_str());
    return 0;
}
