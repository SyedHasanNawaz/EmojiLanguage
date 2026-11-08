#include "ast.h"
#include "interp.h"
#include "codegen.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <cstdio>
#include <string>

extern int yyparse();
extern FILE* yyin;

std::unique_ptr<Program> g_program;

int main(int argc, char* argv[]) {
    bool mode_ir = false;
    bool mode_aot = false;
    std::string output_path = "a.out";
    std::string input_file = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ir") {
            mode_ir = true;
        } else if (arg == "--aot") {
            mode_aot = true;
        } else if (arg == "-o") {
            if (i + 1 < argc) {
                output_path = argv[++i];
            } else {
                std::cerr << "Error: -o option requires an argument\n";
                return 1;
            }
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage:\n";
            std::cout << "  " << argv[0] << " <file.emoji>              Interpret (Phase 1)\n";
            std::cout << "  " << argv[0] << " --ir <file.emoji>         Print LLVM IR to stdout\n";
            std::cout << "  " << argv[0] << " --aot <file.emoji> -o out Compile to native executable\n";
            return 0;
        } else if (input_file.empty()) {
            input_file = arg;
        } else {
            std::cerr << "Error: Unexpected argument: " << arg << "\n";
            return 1;
        }
    }

    FILE* file = nullptr;
    if (!input_file.empty() && input_file != "-") {
        file = std::fopen(input_file.c_str(), "r");
        if (!file) {
            std::cerr << "Error: Could not open file '" << input_file << "'\n";
            return 1;
        }
        yyin = file;
    }

    int parse_res = yyparse();

    if (file) {
        std::fclose(file);
    }

    if (parse_res != 0) {
        return 1;
    }

    if (!g_program) {
        return 0;
    }

    try {
        if (mode_ir) {
            CodeGen codegen;
            codegen.generate(*g_program);
            codegen.dump_ir(std::cout);
        } else if (mode_aot) {
            CodeGen codegen;
            codegen.generate(*g_program);
            if (!codegen.emit_native(output_path)) {
                return 1;
            }
        } else {
            Interpreter interp;
            interp.execute_program(*g_program);
        }
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
