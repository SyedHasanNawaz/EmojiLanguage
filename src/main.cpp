#include "ast.h"
#include "interp.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <cstdio>

extern int yyparse();
extern FILE* yyin;

std::unique_ptr<Program> g_program;

int main(int argc, char* argv[]) {
    FILE* file = nullptr;
    if (argc > 1 && std::string(argv[1]) != "-") {
        file = std::fopen(argv[1], "r");
        if (!file) {
            std::cerr << "Error: Could not open file '" << argv[1] << "'\n";
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
        Interpreter interp;
        interp.execute_program(*g_program);
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
