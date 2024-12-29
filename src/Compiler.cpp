#include "Compiler.h"

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
    std::cout << lex() << std::endl;
    return 0;
}