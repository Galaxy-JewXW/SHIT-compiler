#include "Compiler.h"

int main(int argc, char *argv[]) {
#ifdef SHIT_DEBUG
    log_set_level(LOG_TRACE);
    compiler_options options = parse_args(argc, argv, debug_compile_options);
#else
    log_set_level(LOG_INFO);
    compiler_options options = parse_args(argc, argv);
#endif
    options.print();
    std::ifstream file(options.input_file);
    if (!file) {
        log_fatal("Could not open file %s: %s", options.input_file.c_str(), strerror(errno));
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
    Mir::Module::set_instance(module);
    emit_llvm(module, options._emit_options);

    module->update_id();
    if (options.opt_level >= Optimize_level::O1) {
        execute_O1_passes(module);
    } else {
        execute_O0_passes(module);
    }
    emit_llvm(module, options._emit_options);

    if (options._emit_options.emit_riscv) {
        RISCV::Assembler assembler(module);
        emit_riscv(assembler, options);
    }

    return 0;
}
