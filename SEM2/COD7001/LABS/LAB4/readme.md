# Stack-Based Bytecode Virtual Machine (Lab 4)

## 1. Project Overview
This project implements a functional stack-based Virtual Machine (VM) along with a companion **two-pass assembler**.

The VM supports a custom instruction set including:

- Arithmetic operations  
- Logical comparisons  
- Memory load/store  
- Nested function calls using a dedicated **return stack**

---

## 2. System Requirements

**Compiler:** GCC (GNU Compiler Collection)  
**Interpreter:** Python 3.x (for the assembler)  
**Environment:** Linux, macOS, or WSL (Windows Subsystem for Linux)

---

## 3. Build Instructions

To compile the Virtual Machine:

```bash
gcc main.c -o vm
```

---

## 4. Using the Assembler

The assembler (`asm.py`) converts human-readable `.asm` files into binary bytecode `.bin` files.

**Usage:**

```bash
python3 asm.py <input_file.asm> <output_file.bin>
```

**Example:**

```bash
python3 asm.py fib.asm fib.bin
```

---

## 5. Running the Virtual Machine

To execute a compiled bytecode program:

```bash
./vm <output_file.bin>
```

---

## 6. Included Test Programs

| Program            | Demonstrates                                         | Run Command              |
|-------------------|------------------------------------------------------|--------------------------|
| `simple_fib.bin`  | Basic arithmetic and stack operations                | `./vm simple_fib.bin`    |
| `fib.bin`         | Iterative computation using loops and branching      | `./vm fib.bin`           |
| `nested.bin`      | Function calls, returns, and return-stack handling   | `./vm nested.bin`        |
| `mem_test.bin`    | Memory load/store and loop logic                     | `./vm mem_test.bin`      |

---

## 7. Instruction Set Summary

### Stack Operations
- `PUSH`
- `POP`
- `DUP`

### Arithmetic
- `ADD`
- `SUB`
- `MUL`
- `DIV`
- `CMP`

### Control Flow
- `JMP`
- `JZ`
- `JNZ`

### Memory / Functions
- `STORE`
- `LOAD`
- `CALL`
- `RET`

### System
- `HALT`