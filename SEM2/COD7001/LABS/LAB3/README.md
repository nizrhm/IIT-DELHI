# C-Style Mini-Compiler: Parser & Abstract Syntax Tree (AST)

This project implements a **Lexical Analyzer** and **Syntax Parser** for a C-style subset language. It utilizes **Flex** for tokenization and **Bison** for grammar parsing and AST construction.

## 🚀 Features

* **Variable Declarations:** Supports both uninitialized and initialized declarations (e.g., `var x;`, `var x = 10;`).
* **Control Flow:** Fully nested `if-else` statements and `while` loops with block scoping.
* **Arithmetic Expressions:** Supports standard operations ($+$, $-$, $*$, $/$) with correct operator precedence and parentheses support.
* **Boolean Logic:** Comprehensive comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`).
* **AST Generation:** Constructs a robust, multi-line Abstract Syntax Tree representing the entire program structure.
* **Error Diagnostics:** Accurate line-number reporting for syntax errors using Flex-tracked line counts.

---

## 📂 Project Structure

| File | Description |
| :--- | :--- |
| `src/lexer.l` | Flex lexical rules and token definitions. |
| `src/parser.y` | Bison grammar rules, precedence definitions, and AST node linking. |
| `src/ast.h` | Typedefs and structures for the AST nodes (Statement sequences use sibling pointers). |
| `src/ast.c` | Logic for node creation and recursive tree traversal (printing). |
| `src/main.c` | Driver program that invokes `yyparse()` and manages the output flow. |
| `tests/valid/` | 10 valid test cases demonstrating all language features. |
| `tests/invalid/` | 10 invalid test cases verifying error detection. |

---

## 🛠️ Build and Execution

### Prerequisites
Ensure you have `flex`, `bison`, and `gcc` installed on your system.

### 1. Build the Compiler
Run the following command to generate the parser executable:
```bash
make clean 
```
```bash
make
```

### 2. Build the Compiler
To parse a source file and view the generated AST:
```bash
./parser < tests/valid/test1.txt
```

## 📊 Grammar & Design Methodology

### 🌲 AST Construction

The Abstract Syntax Tree (AST) is designed to support **multiple sequential statements** by using a `next` pointer in each `ASTNode`.

- Each statement node points to the next statement in the sequence
- This forms a linked list of statements inside blocks
- The AST printer traverses the sequence using a `while` loop

This approach simplifies:
- Block handling (`{ ... }`)
- Nested statements
- Program-level statement lists

---

### ⚖️ Ambiguity Handling (Dangling Else)

The language resolves the classic **Dangling Else** ambiguity by assigning **precedence rules** to the `ELSE` token.

```yacc
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE
```
This forces the parser to shift the ELSE token immediately, binding it to the most recent if statement.

## 🧪 Testing Results

### ✅ Valid Input (Complex Nesting)

The parser successfully generates Abstract Syntax Trees (ASTs) for programs containing **deeply nested control-flow constructs**, including multiple levels of `if-else` statements and arithmetic expressions.

Example AST output:
IF_STATEMENT
  CONDITION:
    BINOP: >
      BINOP: * ...
  THEN:
    BLOCK
      IF_STATEMENT
        ...

### ❌ Invalid Input (Error Detection)

The parser accurately detects **syntax errors** in invalid programs and reports the **exact line number** where the error occurs, using line tracking provided by Flex.

Example error output:
Starting Parser...
Syntax Error at line 6: syntax error
Parsing Failed.

## 🏁 Conclusion

The implementation of this **C-style mini-compiler** successfully achieves the core objectives of **lexical analysis** and **syntax parsing**. By integrating **Flex** and **Bison**, the system demonstrates the following strengths:

### 📐 Grammar Precision
The parser accurately distinguishes between complex expression hierarchies and control-flow structures, ensuring that:
- Operator precedence (PEMDAS/BODMAS) is preserved
- Boolean and relational logic is parsed correctly
- Nested constructs are handled without ambiguity

### 🌳 Structural Integrity
By using **sibling (`next`) pointers** in the `ASTNode` structure:
- The compiler supports multi-statement programs
- Entire program modules are represented as a continuous AST
- Block-level and nested statements are naturally modeled

### 🛡️ Robust Error Handling
The system incorporates a fail-safe parsing mechanism:
- Syntax errors are detected immediately
- Accurate line numbers are reported
- Malformed code is prevented from reaching later stages

---

Overall, this project serves as a solid **compiler front-end foundation**, producing a clean and well-structured Abstract Syntax Tree (AST) that is ready for future extensions such as:
- Semantic Analysis
- Intermediate Code Generation
- Direct Interpretation or Code Emission