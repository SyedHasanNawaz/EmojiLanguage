# EmojiLang (Phase 1)

**A Visual, Emoji-Based Programming Language**  
*Official Specification by Faiq (23L-0753) and Hasan (23I-0703)*

Phase 1 implements the complete interpreter for EmojiLang using **Flex** and **Bison** in **C++17**. It executes programs directly via AST interpretation without LLVM, IR, or codegen.

---

## Features

- **Lexer (`src/lexer.l`)**: Recognizes UTF-8 emoji tokens (including optional variation selectors `\uFE0F`), single-line comments (`💬`), identifiers, integer literals, and arithmetic/comparison symbols.
- **Parser (`src/parser.y`)**: Implements the official Bison grammar with operator precedence, half-open slicing, array literals, control structures, and function definitions.
- **AST (`src/ast.h`)**: Clean object-oriented AST representation:
  - Expressions: `NumExpr`, `VarExpr`, `BinOpExpr`, `CallExpr`, `ArrayLitExpr`, `IndexExpr`, `LenExpr`, `SliceExpr`.
  - Statements: `DeclStmt`, `AssignStmt`, `IfStmt`, `WhileStmt`, `FuncDefStmt`, `ReturnStmt`, `PrintStmt`, `ExprStmt`, `BlockStmt`.
- **Interpreter (`src/interp.h`, `src/interp.cpp`)**:
  - `Value = std::variant<long long, std::vector<long long>>`
  - Lexical scoping with `std::vector<std::unordered_map<std::string, Value>>`
  - RAII-based scope guards for exception safety
  - Function call frame isolation and recursive calls
  - Return signal unwinding via `ReturnSignal`
  - Array append semantics with `📥` and array assignment
  - Descriptive runtime error messages with source line numbers

---

## Repository Structure

```text
.
├── CMakeLists.txt
├── README.md
├── run_tests.sh
├── docs/
│   └── specification.md
├── src/
│   ├── ast.h
│   ├── interp.h
│   ├── interp.cpp
│   ├── lexer.l
│   ├── parser.y
│   └── main.cpp
└── tests/
    ├── merge_sort.emoji
    ├── insertion_sort.emoji
    ├── loops.emoji
    ├── recursion.emoji
    ├── arrays_slice.emoji
    └── expected/
        ├── merge_sort.out
        ├── insertion_sort.out
        ├── loops.out
        ├── recursion.out
        └── arrays_slice.out
```

---

## Prerequisites

- Linux / Ubuntu
- `cmake` (>= 3.16)
- `g++` (C++17 support)
- `flex` (>= 2.6)
- `bison` (>= 3.8)

Install dependencies on Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install -y cmake flex bison build-essential
```

---

## Building

```bash
cmake -B build
cmake --build build
```

The resulting executable will be at `./build/emojilang`.

---

## Running Programs

Run an EmojiLang script:
```bash
./build/emojilang tests/merge_sort.emoji
```

Or pass input via stdin:
```bash
./build/emojilang - < tests/recursion.emoji
```

---

## Running Tests

Run the test suite:
```bash
./run_tests.sh
```

All 5 core tests pass against their expected output:
1. `tests/merge_sort.emoji` -> `[3, 9, 10, 27, 38, 43, 82]` (verbatim program from the official PDF)
2. `tests/insertion_sort.emoji` -> `[1, 2, 3, 4, 5]`
3. `tests/loops.emoji` -> prints `0 1 2 3 4`
4. `tests/recursion.emoji` -> `120` (factorial(5))
5. `tests/arrays_slice.emoji` -> `[20, 30]`
