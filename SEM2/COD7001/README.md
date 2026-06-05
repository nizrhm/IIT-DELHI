# COD7001: Software Systems Laboratory

This directory contains labs and projects for **COD7001: Software Systems Laboratory** at IIT Delhi. The coursework covers building core systems components, compiling tools, and operating system utilities from scratch.

---

## 📁 Repository Organization

### 🧪 Labs (Compiler & OS Engineering)

The labs form an incremental track that culminates in a custom language compiler and virtual machine:

* **[LAB 1: Mini UNIX Shell](./LABS/LAB1/)**
  * **Objective:** Build a lightweight UNIX-style command interpreter in C.
  * **Features:** Executing programs in PATH, multi-stage pipelines (`cmd1 | cmd2 | cmd3`), background execution (`&`), input/output redirection (`<`, `>`), and custom history/exit built-ins.
* **[LAB 2: minigdb Debugger](./LABS/LAB2/)**
  * **Objective:** Implement a minimal Linux debugger in C using the `ptrace` system call.
  * **Features:** Breakpoint insertion (replacing bytes with `0xCC` / `INT3`), single-instruction stepping, CPU register inspection (`PTRACE_GETREGS`), and process control (PIE & ASLR compatibility).
* **[LAB 3: Parser & AST Compiler Front-End](./LABS/LAB3/)**
  * **Objective:** Construct a Lexical Analyzer and Syntax Parser for a C-style language subset.
  * **Features:** Built with **Flex** and **Bison**, builds a robust recursive Abstract Syntax Tree (AST), handles Dangling Else ambiguity, and reports grammar syntax errors with precise line numbers.
* **[LAB 4: Stack-Based Bytecode VM](./LABS/LAB4/)**
  * **Objective:** Design a custom instruction-set VM runtime and a companion 2-pass Python assembler.
  * **Features:** Supports arithmetic/logic stack ops, memory load/stores, control-flow branching, and nested functions using a return stack.
* **[LAB 5: Mark-Sweep Garbage Collector](./LABS/LAB5/)**
  * **Objective:** Integrate an autonomous memory reclamation engine into the bytecode VM heap.
  * **Features:** Reachability-based tracing, an iterative marking worklist (avoiding stack overflows on deep graphs), and a self-scaling threshold logic (`maxObjects = currentObjects * 2`).
* **[LAB 6: Integrated Compiler System](./LABS/LAB6/)**
  * **Objective:** End-to-end integration of Labs 1–5.
  * **Features:** Compiles high-level C-style code from Lab 3 directly into custom VM bytecode from Lab 4 and runs it on the VM using the mark-sweep GC from Lab 5.

---

### 🎓 Final Project Showcase

The final project files are located in:
* **[PROJECT](./PROJECT/)**
  * Contains the final research presentation: `COD7001-3PAGER.pptx`
  * Contains the official Technical Project Report: `COD7001_Project_REPORT.pdf`
  * Contains the project demonstration video: `DEMO VID_BY ASLAM.mp4`

*(Note: The project source code could not be retrieved, but all reports and demo materials are archived here for evaluation).*

---
[← Semester 2 Index](../README.md) | [← Portfolio Root](../../README.md)
