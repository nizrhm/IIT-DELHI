# README: VM with Mark-Sweep Garbage Collector (Lab 5)

## 📋 Project Overview

This project implements a high-performance, heap-based Virtual Machine (VM) integrated with an autonomous **Mark-and-Sweep Garbage Collector**. The system is designed to handle industrial-scale object management by automating memory reclamation based on reachability from the root set (stack and globals).

---

## 🚀 Architectural Features

* 
**Heap-Based VM Runtime**: The entire VM structure, including the 1,048,576-slot operand stack, is allocated on the heap using `malloc`. This design bypasses the default 8MB Linux system stack limit.


* 
**Iterative Worklist Marking**: To avoid segmentation faults during deep object graph traversal, the marking phase uses a manual, heap-allocated worklist instead of recursive calls.


* 
**Linked-List Object Tracking**: All allocated objects are tracked via a global linked list, allowing for a linear-time sweep phase.


* 
**Self-Scaling Threshold**: The GC trigger dynamically scales based on the current workload using the formula: `maxObjects = currentObjects * 2`.



---

## 🛠️ Build and Execution

### Prerequisites

* 
**Compiler**: `gcc` (GNU Compiler Collection).


* **Assembler**: `python3` (for `asm.py`).
* 
**Environment**: Linux/Unix-based lab environment.



### 1. Build the VM

Use the provided `Makefile` to compile the source code:

```bash
make clean
make

```

### 2. Assemble Test Scripts

Convert your assembly (`.asm`) files into VM-readable binaries (`.bin`):

```bash
python3 asm.py tests/gc_test.asm gc_test.bin

```

### 3. Run the VM

Execute the generated binary:

```bash
./vm gc_test.bin

```

---

## 🧪 Testing & Validation

The implementation has been verified against a rigorous suite of test cases to ensure correctness and stability:

| Test Case | Description | Expected Outcome |
| --- | --- | --- |
| **Basic Reachability** | Objects directly on the stack are preserved.

 | Object survives GC.

 |
| **Unreachable Collection** | Reclaims objects with no references.

 | Object is freed; heap is empty.

 |
| **Cyclic References** | Handles self-referential structures without infinite loops.

 | Objects survive; no crash.

 |
| **Deep Object Graph** | Stress-tests recursive marking with 10,000+ objects.

 | All objects survive; no stack overflow.

 |
| **Stress Allocation** | Heavy allocation (100k+ objects) to test heap integrity.

 | Heap is empty after GC; no leaks.

 |

---

## 📊 Performance Analysis

* 
**Functional Verification**: The VM correctly identifies and reclaims temporary objects while preserving live values on the stack.


* **Memory Safety**: Analysis with **Valgrind Memcheck** confirms:
* 
**0 errors** from 0 contexts.


* 
**0 leaks**; all heap blocks were successfully freed.




* 
**Adaptive Scaling**: The adaptive trigger logic functions as expected, reclaiming thousands of objects while scaling thresholds dynamically.



---

## 📂 Project Structure

* `main.c`: Core VM runtime and Mark-Sweep logic.
* `asm.py`: Two-pass assembler for the custom ISA.
* `tests/`: Directory containing `.asm` scripts for various GC scenarios.
* `Cornerstone_5.pdf`: Technical report detailing architectural decisions and results.

---

## 🏁 Author Information

* 
**Mayur Kamble** (2025MCS2116).


* 
**Nizaul Rahman** (2025MCS2099).


* 
**Course**: COD7001 - Compiler Construction/Systems Programming Labs.