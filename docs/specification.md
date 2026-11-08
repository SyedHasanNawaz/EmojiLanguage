# EmojiLang Language Specification (Phase 1)

EmojiLang is an educational, emoji-based programming language designed by Faiq (23L-0753) and Hasan (23I-0703). Phase 1 implements a direct-execution Flex + Bison tree-walk interpreter in C++17 without IR or code generation.

## 1. Reserved Tokens and Keywords

| Token / Emoji | Name | Description | Example |
| :--- | :--- | :--- | :--- |
| `🔢` | Decl | Variable declaration | `🔢 i = 0` |
| `📥` | Assign | Value assignment / append | `📥 result = 📦[🔍(left, i)]` |
| `🤔` | If | Conditional if statement | `🤔 📏(arr) <= 1 { ... }` |
| `😢` | Else | Else branch | `😢 { ... }` |
| `🔁` | Loop | While loop form | `🔁 i < 📏(left) { ... }` |
| `⚙️` / `⚙` | FuncDef | Function definition | `⚙️ merge(left, right) { ... }` |
| `↩️` / `↩` | Return | Return statement | `↩️ result` |
| `📢` | Print | Print expression | `📢 sorted` |
| `📦` | ArrayLit | Array literal constructor | `📦[38, 27, 43, 3, 9, 82, 10]` |
| `🔍` | Index | Array index access | `🔍(arr, i)` |
| `📏` | Length | Array length | `📏(arr)` |
| `✂️` / `✂` | Slice | Array half-open sub-array `[start, end)` | `✂️(arr, 0, mid)` |
| `➕`, `+` | Plus | Addition / Array concatenation | `a + b` |
| `➖`, `-` | Minus | Subtraction / Unary negation | `a - b`, `-a` |
| `✖️`, `✖`, `*` | Multiply | Multiplication | `a * b` |
| `➗`, `/` | Divide | Integer division | `a / b` |
| `%` | Modulo | Integer modulo | `a % b` |
| `<`, `>`, `<=`, `>=` | Relational | Comparison operators | `i < 📏(left)` |
| `==`, `!=`, `⚖️`, `⚖` | Equality | Equality comparisons | `a == b` |
| `&&`, `||` | Logical | Short-circuit logical AND / OR | `a && b` |
| `{` `}` | Block | Block delimiters | `{ ... }` |
| `💬` | Comment | Single-line comment to end of line | `💬 Comment` |

## 2. Statement Syntax & Termination

Statements end at newlines or before `}`. Semicolons `;` are optional.
Statements supported:
- **Variable Declaration**: `🔢 ID '=' expr`
- **Assignment**: `📥 ID '=' expr` or bare `ID '=' expr`
  - When `📥` is used and the target variable is an array, `📥 arr = val` appends to the array.
  - Bare assignment `ID = expr` overwrites the target variable.
- **Conditional**: `🤔 expr '{' stmts '}' ('😢' '{' stmts '}')?`
- **Loop**: `🔁 expr '{' stmts '}'`
- **Function Definition**: `⚙️ ID '(' params ')' '{' stmts '}'`
- **Return Statement**: `↩️ expr`
- **Print Statement**: `📢 expr`
- **Expression Statement**: `expr`

## 3. Data Types & Semantics

- `Value` is `std::variant<long long, std::vector<long long>>`.
- **Truthiness**: Integers only; non-zero is true, zero is false. Conditions evaluating to non-integers throw runtime errors.
- **Functions**: Stored in a function symbol table. Scopes are lexical with isolated function frames and global access. Return statements throw a `ReturnSignal` caught at function boundaries.
- **Error Handling**: Descriptive errors with source line numbers:
  - Undefined variable / function
  - Array index out of bounds
  - Slice type mismatch
  - Division / modulo by zero
  - Type errors in conditions and arithmetic operations
