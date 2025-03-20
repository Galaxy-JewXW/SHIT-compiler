#ifndef COMPILER_H
#define COMPILER_H

// ReSharper disable CppUnusedIncludeDirective
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Frontend/Lexer.h"
#include "Frontend/Parser.h"
#include "Mir/Builder.h"
#include "Mir/Value.h"
#include "Pass/Analysis.h"
#include "Pass/Transform.h"
#include "Pass/Util.h"
#include "Utils/Log.h"
#include "Backend/Assembler.h"

enum class Optimize_level { O0, O1, O2 };

const Optimize_level default_opt_level = Optimize_level::O0;

struct emit_options {
    bool emit_tokens = false;
    std::string tokens_file;
    bool emit_ast = false;
    std::string ast_file;
    bool emit_llvm = false;
    std::string llvm_file;
    bool emit_riscv = false;
};

struct compiler_options {
    std::string input_file;
    bool flag_S = false;
    std::string output_file;
    emit_options _emit_options;
    Optimize_level opt_level = default_opt_level;

    void print() const;
};

// cmake设置为Debug时的编译选项
extern const compiler_options debug_compile_options;

template<typename T>
void emit_output(const std::string &filename, const T &content) {
    std::ostream *out = &std::cout;
    std::ofstream file_stream;
    if (!filename.empty()) {
        file_stream.open(filename);
        if (!file_stream) {
            log_error("Failed to open file: %s", filename.c_str());
        }
        out = &file_stream;
    }
    *out << content << std::endl;
    if (file_stream.is_open()) {
        file_stream.close();
    }
}

void emit_tokens(const std::vector<Token::Token> &tokens, const emit_options &options);

void emit_ast(const std::shared_ptr<AST::CompUnit> &ast, const emit_options &options);

void emit_llvm(const std::shared_ptr<Mir::Module> &module, const emit_options &options);

void emit_riscv(const std::shared_ptr<Assembler::riscv_assembler> &assembler, const emit_options &options);

void usage(const char *prog_name);

compiler_options parse_args(int argc, char *argv[]);

compiler_options parse_args(const int argc, char *argv[], compiler_options options);

int main(int, char *[]);

#endif
