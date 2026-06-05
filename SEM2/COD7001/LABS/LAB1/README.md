# Mini UNIX Shell — User Guide

## 1. Introduction

This project implements a lightweight UNIX-style command-line shell written in C.  
It supports executing external commands, pipelines, redirection, background execution, and built-in commands—similar to a simplified Bash shell.

The shell runs inside a terminal and displays the prompt:

nizshell>

Users can type commands exactly as they would in a normal UNIX shell.

## 2. Features Overview

### ✔ Mandatory Features

| Requirement               | Description                                                   | Status |
|---------------------------|--------------------------------------------------------------- --------|
| Execute programs via PATH | Supports running any command available in PATH (`ls`, `grep`, `python3`, etc.) | ✔ |
| Argument parsing + quotes | Handles `"strings with spaces"` and `'single quotes'`         | ✔ |
| I/O redirection           | `< input.txt` and `> output.txt`                              | ✔ |
| Single pipeline           | `cmd1 \| cmd2` works                                          | ✔ |
| Background execution      | `cmd &` works                                                 | ✔ |
| Built-ins: `cd`, `exit`   | Implemented directly in shell                                 | ✔ |

### ✔ Advanced Features (Optional)

| Feature               | Example       | Status |
|-----------------------|---------------|--------|
| History command       | `history`     | ✔ |
| Multi-stage pipelines | `cmd1 \| cmd2`| ✔ |
| Robust signal handling| `Ctrl-C` does not kill the shell | ✔ |
| Zombie cleanup        | Shell does not leave orphan processes | ✔ |


## 3. Running the Shell
Build: make
Run: ./nizshell

You will see: nizshell>

## 4. Usage Guide
### 4.1 Running Commands

Examples:

nizshell> ls
nizshell> pwd
nizshell> whoami
nizshell> echo hello world

### 4.2 Quoted Arguments
nizshell> echo "hello world"
hello world

nizshell> echo 'this is a test'
this is a test

### 4.3 Input Redirection <

Reads input from a file:

nizshell> sort < names.txt

### 4.4 Output Redirection >

Writes output to a file:

nizshell> ls > files.txt

### 4.5 Pipelines |

Single or multi-stage:

nizshell> ls | wc -l

nizshell> cat file.txt | grep hello | sort | uniq

### 4.6 Background Execution &

Run commands without blocking:

nizshell> long_task &
[bg pid 2451]

### 4.7 Built-ins
cd
nizshell> cd /home

history
nizshell> history
   1 ls
   2 pwd
   3 history

exit

Ends the shell: nizshell> exit

## 5. Error Handling

Your shell safely handles:

Missing files

Unknown commands

Too many arguments

Empty pipeline stages

Invalid redirection

Ctrl-C

Zombie processes

Examples:

nizshell> nosuchcmd
execvp: No such file or directory

## 6. Design Summary (User-Level Explanation)
Tokenizer

Breaks the input into:

words

quoted strings

control symbols: <, >, |, &

Parser

### Builds an array of Command structures:

argv list

input file

output file

### Supports up to 16 pipeline commands.

Executor

For a single command → fork + execvp

For pipelines → builds N pipes and forks N children

Applies redirection before execvp

Handles background vs foreground

Signals

Shell ignores Ctrl-C

Children restore default behavior

SIGCHLD reaps zombies

## 7. Known Limitations

To keep the shell simple:

No job control (fg/bg move jobs)

No wildcard expansion (*.c)

No tab completion

No environment variable expansion ($HOME)

These are optional in the future.

## 8. Screenshots

Screenshots inside the folder: screenshots/

## 9. How to Run Test Suite

In the tests/ folder, run:

bash tests.txt


Or run manually:

nizshell> cat nizshell.c | tr a-z A-Z | head -n 5
nizshell> echo "one two three" | tr ' ' '\n' | wc -l
nizshell> ls | grep .c | wc -l

## 10. Conclusion

This mini-shell is a fully functional UNIX-style command interpreter that implements all required features and additional advanced capabilities. It demonstrates understanding of:

process creation

pipes

signal handling

redirection

parsing

shell architecture