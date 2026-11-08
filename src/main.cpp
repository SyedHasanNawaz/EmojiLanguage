#include "ast.h"
#include "interp.h"
#include "codegen.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <cstdio>
#include <string>

using namespace std;

extern int yyparse();
extern FILE* yyin;

unique_ptr<Program> g_program;

int main(int argc, char* argv[]) {
    bool is_ir = false;
    bool is_aot = false;
    string out_file = "a.out";
    string in_file = "";

    for (int i = 1; i < argc; ++i) {
        string s = argv[i];
        if (s == "--ir") {
            is_ir = true;
        } 
        else if (s == "--aot") {
            is_aot = true;
        } 
        else if (s == "-o") {
            if (i + 1 < argc) {
                out_file = argv[++i];
            } else {
                cerr << "Error: -o option requires an argument\n";
                return 1;
            }
        } 
        else if (s == "-h" || s == "--help") {
            cout << "Usage:\n";
            cout << "  " << argv[0] << " <file.emoji>\n";
            cout << "  " << argv[0] << " --ir <file.emoji>\n";
            cout << "  " << argv[0] << " --aot <file.emoji> -o out\n";
            return 0;
        } 
        else if (in_file.empty()) {
            in_file = s;
        } 
        else {
            cerr << "Error: Unexpected argument: " << s << "\n";
            return 1;
        }
    }

    FILE* f = nullptr;
    if (!in_file.empty() && in_file != "-") {
        f = fopen(in_file.c_str(), "r");
        if (!f) {
            cerr << "Error: Could not open file '" << in_file << "'\n";
            return 1;
        }
        yyin = f;
    }

    int res = yyparse();

    if (f) {
        fclose(f);
    }

    if (res != 0) {
        return 1;
    }

    if (!g_program) {
        return 0;
    }

    try {
        if (is_ir) {
            CodeGen cg;
            cg.generate(*g_program);
            cg.dump_ir(cout);
        } 
        else if (is_aot) {
            CodeGen cg;
            cg.generate(*g_program);
            if (!cg.emit_native(out_file)) {
                return 1;
            }
        } 
        else {
            Interpreter interp;
            interp.execute_program(*g_program);
        }
    } catch (const runtime_error& e) {
        cerr << e.what() << "\n";
        return 1;
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
