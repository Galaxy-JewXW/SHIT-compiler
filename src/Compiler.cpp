#include "Compiler.h"

int main(int argc, char *argv[]) {
#ifdef SHIT_DEBUG
    log_set_level(LOG_TRACE);
    compiler_options options = debug_compile_options;
#else
    log_set_level(LOG_INFO);
    compiler_options options = parse_args(argc, argv);
#endif
    options.print();
    std::ifstream file(options.input_file);
    if (!file.is_open()) {
        log_fatal("Could not open file: %s", options.input_file.c_str());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string src_code = buffer.str();
    file.close();

    Lexer lexer(src_code);
    const std::vector<Token::Token> &tokens = lexer.tokenize();
    emit_tokens(tokens, options._emit_options);

    Parser parser(tokens);
    std::shared_ptr<AST::CompUnit> ast = parser.parse();
    emit_ast(ast, options._emit_options);

    Mir::Builder builder;
    std::shared_ptr<Mir::Module> module = builder.visit(ast);
    emit_llvm(module, options._emit_options);

    if (options.flag_S) {
        // TODO
    }

    return 0;
}
