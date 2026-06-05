# Technical Report: LAB 2 - Implementation of a Minimal Debugger  
### Course Code: COD7001

**Mayur Kamble** — 2025MCS2116  
**Nizaul Rahman** — 2025MCS2099

---

## 1. Introduction and Project Goal

This report describes the design and implementation of a minimal debugger, named **minigdb**, developed for ELF binaries on Linux. The primary objective of this project is to implement essential debugging functionality using operating-system-level mechanisms, specifically the `ptrace` system call, without relying on external debugging tools such as GDB.

The debugger supports controlled execution of a target program, breakpoint management, instruction-level stepping, register inspection, and process state reporting. The implementation focuses on correctness, clarity, and adherence to low-level OS concepts.

### Implemented Mandatory Functionalities

- Loading and executing a target binary  
- Setting and removing software breakpoints  
- Single-step execution and continue execution  
- Inspecting CPU registers  
- Displaying process execution status  

### Implemented Optional Extensions

- Memory inspection  
- Interactive debugger command shell  

---

## 2. Architectural Design: The Ptrace-Based Debugging Model

The debugger follows the classic **parent–child tracing model** provided by Linux through `ptrace`.

### 2.1 Process Creation and Attachment

1. The debugger forks to create a child process (the debuggee).
2. The child invokes `ptrace(PTRACE_TRACEME)` to allow tracing.
3. The child replaces its image using `execl()` to execute the target binary.
4. The kernel delivers a `SIGTRAP` after `execve`, stopping the child.
5. The parent debugger waits for and manages the child using `waitpid()`.

This mechanism establishes full control of the debuggee by the debugger.

---

## 3. Software Breakpoints

Breakpoints are implemented using **software traps**, a standard debugging technique.

### 3.1 Breakpoint Insertion

To set a breakpoint at a given address:

1. The original instruction word is read using `PTRACE_PEEKTEXT`.
2. The least significant byte is saved.
3. The byte is replaced with the `INT3` instruction (`0xCC`) using `PTRACE_POKETEXT`.

This causes the program to raise a `SIGTRAP` when execution reaches the breakpoint.

### 3.2 Breakpoint Handling and Step-Over

When a breakpoint is hit:

1. The instruction pointer (RIP) is decremented by one.
2. The original instruction byte is restored.
3. A single instruction is executed using `PTRACE_SINGLESTEP`.
4. The breakpoint instruction (`0xCC`) is reinserted.

This ensures correct execution flow while preserving breakpoint behavior.

---

## 4. Execution Control and Inspection

### 4.1 Execution Control

The debugger provides two execution control modes:

- **Continue execution** using `PTRACE_CONT`
- **Single-step execution** using `PTRACE_SINGLESTEP`

These allow precise control over program execution at the instruction level.

### 4.2 Register Inspection

The debugger retrieves CPU register values using `PTRACE_GETREGS`.  
Key registers such as RIP, RSP, RBP, and general-purpose registers are displayed to the user at breakpoints or after stepping.

### 4.3 Process Status Reporting

The debugger interprets process state using `waitpid()` macros:

- Normal exit  
- Termination by signal  
- Stop due to breakpoint or external signal  

This provides clear feedback on the debuggee’s current state.

---

## 5. Addressing PIE and ASLR (Design Consideration)

Modern Linux systems enable **Position Independent Executables (PIE)** and **Address Space Layout Randomization (ASLR)**, which cause executables to be loaded at randomized base addresses.

The current debugger implementation expects **absolute runtime addresses** when setting breakpoints. Users may obtain these addresses externally (e.g., via `/proc/<pid>/maps` or auxiliary tools).

This design choice keeps the debugger minimal while remaining compatible with PIE-enabled binaries when correct runtime addresses are supplied.

---

## 6. Optional Extensions

### 6.1 Memory Inspection

The debugger supports memory inspection using `PTRACE_PEEKDATA`.

Command format:

x /<count> 0xADDRESS


This allows the user to read and display memory contents starting from a specified address for a given number of bytes.

### 6.2 Interactive Debugger Shell

An interactive command loop is implemented to accept user commands such as:

- `break 0xADDRESS`
- `cont`
- `step` / `s`
- `info regs`
- `status`
- `x /count 0xADDRESS`
- `quit`

Input parsing includes whitespace trimming and basic argument handling to ensure robustness.

---

## 7. Conclusion

The **minigdb** debugger successfully demonstrates core debugging functionality using only the Linux `ptrace` interface. The project showcases understanding of:

- Process tracing and control
- Software breakpoints
- Instruction-level execution
- Register and memory inspection
- Signal and process state handling

This implementation provides a comp# Technical Report: Implementation of a Minimal Debugger  
### Course Code: COD7001

**Mayur Kamble** — 2025MCS2116  
**Nizaul Rahman** — 2025MCS2099

---

## 1. Introduction and Project Goal

This report describes the design and implementation of a minimal debugger, named **minigdb**, developed for ELF binaries on Linux. The primary objective of this project is to implement essential debugging functionality using operating-system-level mechanisms, specifically the `ptrace` system call, without relying on external debugging tools such as GDB.

The debugger supports controlled execution of a target program, breakpoint management, instruction-level stepping, register inspection, and process state reporting. The implementation focuses on correctness, clarity, and adherence to low-level OS concepts.

### Implemented Mandatory Functionalities

- Loading and executing a target binary  
- Setting and removing software breakpoints  
- Single-step execution and continue execution  
- Inspecting CPU registers  
- Displaying process execution status  

### Implemented Optional Extensions

- Memory inspection  
- Interactive debugger command shell  

---

## 2. Architectural Design: The Ptrace-Based Debugging Model

The debugger follows the classic **parent–child tracing model** provided by Linux through `ptrace`.

### 2.1 Process Creation and Attachment

1. The debugger forks to create a child process (the debuggee).
2. The child invokes `ptrace(PTRACE_TRACEME)` to allow tracing.
3. The child replaces its image using `execl()` to execute the target binary.
4. The kernel delivers a `SIGTRAP` after `execve`, stopping the child.
5. The parent debugger waits for and manages the child using `waitpid()`.

This mechanism establishes full control of the debuggee by the debugger.

---

## 3. Software Breakpoints

Breakpoints are implemented using **software traps**, a standard debugging technique.

### 3.1 Breakpoint Insertion

To set a breakpoint at a given address:

1. The original instruction word is read using `PTRACE_PEEKTEXT`.
2. The least significant byte is saved.
3. The byte is replaced with the `INT3` instruction (`0xCC`) using `PTRACE_POKETEXT`.

This causes the program to raise a `SIGTRAP` when execution reaches the breakpoint.

### 3.2 Breakpoint Handling and Step-Over

When a breakpoint is hit:

1. The instruction pointer (RIP) is decremented by one.
2. The original instruction byte is restored.
3. A single instruction is executed using `PTRACE_SINGLESTEP`.
4. The breakpoint instruction (`0xCC`) is reinserted.

This ensures correct execution flow while preserving breakpoint behavior.

---

## 4. Execution Control and Inspection

### 4.1 Execution Control

The debugger provides two execution control modes:

- **Continue execution** using `PTRACE_CONT`
- **Single-step execution** using `PTRACE_SINGLESTEP`

These allow precise control over program execution at the instruction level.

### 4.2 Register Inspection

The debugger retrieves CPU register values using `PTRACE_GETREGS`.  
Key registers such as RIP, RSP, RBP, and general-purpose registers are displayed to the user at breakpoints or after stepping.

### 4.3 Process Status Reporting

The debugger interprets process state using `waitpid()` macros:

- Normal exit  
- Termination by signal  
- Stop due to breakpoint or external signal  

This provides clear feedback on the debuggee’s current state.

---

## 5. Addressing PIE and ASLR (Design Consideration)

Modern Linux systems enable **Position Independent Executables (PIE)** and **Address Space Layout Randomization (ASLR)**, which cause executables to be loaded at randomized base addresses.

The current debugger implementation expects **absolute runtime addresses** when setting breakpoints. Users may obtain these addresses externally (e.g., via `/proc/<pid>/maps` or auxiliary tools).

This design choice keeps the debugger minimal while remaining compatible with PIE-enabled binaries when correct runtime addresses are supplied.

---

## 6. Optional Extensions

### 6.1 Memory Inspection

The debugger supports memory inspection using `PTRACE_PEEKDATA`.

Command format:

x /<count> 0xADDRESS


This allows the user to read and display memory contents starting from a specified address for a given number of bytes.

### 6.2 Interactive Debugger Shell

An interactive command loop is implemented to accept user commands such as:

- `break 0xADDRESS`
- `cont`
- `step` / `s`
- `info regs`
- `status`
- `x /count 0xADDRESS`
- `quit`

Input parsing includes whitespace trimming and basic argument handling to ensure robustness.

---

## 7. Conclusion

The **minigdb** debugger successfully demonstrates core debugging functionality using only the Linux `ptrace` interface. The project showcases understanding of:

- Process tracing and control
- Software breakpoints
- Instruction-level execution
- Register and memory inspection
- Signal and process state handling

This implementation provides a compact yet powerful educational debugger and meets all mandatory project requirements while incorporating useful optional extensions.
