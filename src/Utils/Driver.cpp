#include "Compiler.h"

compiler_options debug_compile_options = {
    .input_file = "../testcase.sy",
    .flag_S = true,
    .output_file = "../testcase.s",
    ._emit_options = {.emit_tokens = false, .emit_ast = false, .emit_llvm = true},
    .opt_level = Optimize_level::O1
};

std::string opt_level_to_string(const Optimize_level level) {
    switch (level) {
        case Optimize_level::O0: return "O0";
        case Optimize_level::O1: return "O1";
        case Optimize_level::O2: return "O2";
    }
    return "unknown";
}

void compiler_options::print() const {
    std::stringstream ss;
    ss << "Options: "
            << "-input=" << input_file
            << ", -S=" << (flag_S ? "true" : "false");
    if (!output_file.empty()) {
        ss << ", -output=" << output_file;
    }
    ss << ", opt=-" << opt_level_to_string(opt_level);
    if (_emit_options.emit_tokens) {
        ss << ", -emit-tokens="
                << (_emit_options.tokens_file.empty() ? "stdout" : _emit_options.tokens_file);
    }
    if (_emit_options.emit_ast) {
        ss << ", -emit-ast="
                << (_emit_options.ast_file.empty() ? "stdout" : _emit_options.ast_file);
    }
    if (_emit_options.emit_llvm) {
        ss << ", -emit-llvm="
                << (_emit_options.llvm_file.empty() ? "stdout" : _emit_options.llvm_file);
    }
    log_info("%s", ss.str().c_str());
}


void usage(const char *prog_name) {
    std::cout << "Usage: " << prog_name << " input-file [options]\n"
            << "Options:\n"
            << "  -S                      Generate assembly output\n"
            << "  -o <output>             Specify output file for assembly\n"
            << "  -O0                     Basic optimization (default)\n"
            << "  -O1                     Advanced optimizations\n"
            << "  -O2                     Radical optimizations\n"
            << "  -emit-tokens [<file>]   Output tokens to file or (default) stdout\n"
            << "  -emit-ast [<file>]      Output AST to file or (default) stdout\n"
            << "  -emit-llvm [<file>]     Output LLVM IR to file or (default) .ll file\n";
}

compiler_options parse_args(const int argc, char *argv[]) {
    compiler_options options;
    if (argc < 2) {
        usage(argv[0]);
        log_fatal("No input file specified");
    }
    int i = 1;
    while (i < argc) {
        std::string arg = argv[i];
        if (arg[0] == '-') {
            if (arg == "-S") {
                options.flag_S = true;
                i++;
            } else if (arg == "-o") {
                if (i + 1 >= argc || argv[i + 1][0] == '-') {
                    usage(argv[0]);
                    log_fatal("Missing output file after -o");
                }
                options.output_file = argv[i + 1];
                i += 2;
            } else if (arg == "-O0") {
                options.opt_level = Optimize_level::O0;
                i++;
            } else if (arg == "-O1") {
                options.opt_level = Optimize_level::O1;
                i++;
            } else if (arg == "-O2") {
                options.opt_level = Optimize_level::O2;
                i++;
            } else if (arg == "-emit-tokens") {
                options._emit_options.emit_tokens = true;
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    options._emit_options.tokens_file = argv[i + 1];
                    i += 2;
                } else {
                    i++;
                }
            } else if (arg == "-emit-ast") {
                options._emit_options.emit_ast = true;
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    options._emit_options.ast_file = argv[i + 1];
                    i += 2;
                } else {
                    i++;
                }
            } else if (arg == "-emit-llvm") {
                options._emit_options.emit_llvm = true;
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    options._emit_options.llvm_file = argv[i + 1];
                    i += 2;
                } else {
                    i++;
                }
            } else {
                usage(argv[0]);
                log_fatal("Unknown option: %s", arg.c_str());
            }
        } else {
            if (!options.input_file.empty()) {
                usage(argv[0]);
                log_fatal("Multiple input files specified");
            }
            options.input_file = arg;
            i++;
        }
    }
    if (options.input_file.empty()) {
        usage(argv[0]);
        log_fatal("No input file specified");
    }
    if (options._emit_options.emit_llvm && options._emit_options.llvm_file.empty()) {
        const size_t last_dot = options.input_file.find_last_of('.');
        options._emit_options.llvm_file = options.input_file.substr(0, last_dot) + ".ll";
    }
    if (options.flag_S && options.output_file.empty()) {
        const size_t last_dot = options.input_file.find_last_of('.');
        options.output_file = options.input_file.substr(0, last_dot) + ".s";
    }

    return options;
}

void emit_tokens(const std::vector<Token::Token> &tokens, const emit_options &options) {
    if (!options.emit_tokens) return;
    log_info("Emitting tokens...");
    emit_output(options.tokens_file,
                [&tokens]() -> std::string {
                    std::stringstream ss;
                    for (const auto &token: tokens) {
                        ss << token.to_string() << std::endl;
                    }
                    return ss.str();
                }());
}

void emit_ast(const std::shared_ptr<AST::CompUnit> &ast, const emit_options &options) {
    if (!options.emit_ast) return;
    log_info("Emitting AST...");
    emit_output(options.ast_file, ast->to_string());
}

void emit_llvm(const std::shared_ptr<Mir::Module> &module, const emit_options &options) {
    if (!options.emit_llvm) return;
    log_info("Emitting LLVM IR...");
    module->update_id();
    emit_output(options.llvm_file, module->to_string());
}
