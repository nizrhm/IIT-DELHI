import subprocess
import os
import sys
import time
import shutil

# Configuration
SHELL_BIN = "./mysystem"
TEST_DIR = "./tests/tests_50"
LOG_FILE = "./tests/test_suite_50.log"

# Colors
GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
BLUE = "\033[94m"
RESET = "\033[0m"

# --- HELPER FUNCTIONS ---

def setup():
    if os.path.exists(TEST_DIR): shutil.rmtree(TEST_DIR)
    os.makedirs(TEST_DIR)
    
    # Check if binary exists
    if not os.path.exists(SHELL_BIN):
        print(f"{RED}[CRITICAL] {SHELL_BIN} not found. Running make...{RESET}")
        subprocess.run(["make", "-C", "src", "clean"], stdout=subprocess.DEVNULL)
        subprocess.run(["make", "-C", "src"], stdout=subprocess.DEVNULL)
        if not os.path.exists(SHELL_BIN):
            print(f"{RED}[FATAL] Build failed.{RESET}")
            sys.exit(1)

def create_file(name, content):
    path = os.path.join(TEST_DIR, name)
    with open(path, "w") as f:
        f.write(content)
    return path

def run_shell(input_str, timeout=2):
    try:
        process = subprocess.Popen(
            [SHELL_BIN],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        stdout, stderr = process.communicate(input=input_str + "\nexit\n", timeout=timeout)
        return (stdout + stderr).replace("\r", "")
    except subprocess.TimeoutExpired:
        process.kill()
        return "[TIMEOUT]"

def assert_contains(output, expected):
    if expected in output: return True
    return False

def assert_not_contains(output, unexpected):
    if unexpected not in output: return True
    return False

# --- TEST GENERATORS ---

results = []

def run_case(id, category, description, input_cmd, expected, unexpected=None):
    print(f"Test {id:02d} [{category}]: {description} ...", end=" ")
    output = run_shell(input_cmd)
    
    passed = True
    reason = ""
    
    if output == "[TIMEOUT]":
        passed = False
        reason = "System Timeout/Hang"
    elif not assert_contains(output, expected):
        passed = False
        reason = f"Missing expected output: '{expected}'"
    elif unexpected and not assert_not_contains(output, unexpected):
        passed = False
        reason = f"Found unexpected output: '{unexpected}'"
        
    if passed:
        print(f"{GREEN}PASS{RESET}")
        results.append({"id": id, "status": "PASS", "desc": description})
    else:
        print(f"{RED}FAIL{RESET}")
        results.append({"id": id, "status": "FAIL", "desc": description, "reason": reason})
        with open(LOG_FILE, "a") as f:
            f.write(f"\n=== TEST {id} FAIL ===\nInput:\n{input_cmd}\nOutput:\n{output}\nReason: {reason}\n")

# --- MAIN EXECUTION ---

def main():
    setup()
    with open(LOG_FILE, "w") as f: f.write("--- 50 TEST ROBUSTNESS SUITE ---\n")
    
    print(f"{BLUE}=== STARTING 50-TEST ROBUSTNESS SUITE ==={RESET}\n")

    # =========================================================================
    # CATEGORY 1: LAB 1 SHELL BASICS (OS COMMANDS)
    # =========================================================================
    run_case(1, "Shell", "Standard 'ls' command", "ls", "Makefile")
    run_case(2, "Shell", "Standard 'pwd' command", "pwd", "/cstn6")
    run_case(3, "Shell", "Echo with spaces", "echo hello world", "hello world")
    run_case(4, "Shell", "Unknown OS command", "flibberflabber", "not found") # Expect error
    run_case(5, "Shell", "Empty input line", "\n\n", "myshell>") # Should just reprompt
    run_case(6, "Shell", "Exit command", "exit", "Exiting system")
    
    # =========================================================================
    # CATEGORY 2: COMPILER / PARSER (VALID)
    # =========================================================================
    f_valid = create_file("valid.lang", "var x = 10; var y = 20; if (x < y) { x = x + 1; }")
    run_case(7, "Parser", "Submit valid file", f"submit {f_valid}", "PID: 1")
    
    f_math = create_file("math.lang", "var x = 5 + 3 * 2;") 
    run_case(8, "Parser", "Operator Precedence", f"submit {f_math}", "PID: 1")
    
    f_loop = create_file("loop.lang", "var i = 0; while (i < 5) { i = i + 1; }")
    run_case(9, "Parser", "While Loop Syntax", f"submit {f_loop}", "PID: 1")
    
    f_nested = create_file("nested.lang", "if (1) { if (1) { var x = 1; } }")
    run_case(10, "Parser", "Nested Blocks", f"submit {f_nested}", "PID: 1")

    # =========================================================================
    # CATEGORY 3: COMPILER / PARSER (INVALID - ROBUSTNESS)
    # =========================================================================
    f_err1 = create_file("err_semi.lang", "var x = 10") # No semi
    run_case(11, "Parser", "Missing Semicolon", f"submit {f_err1}", "Syntax Error", "PID: 1")
    
    f_err2 = create_file("err_brace.lang", "if (x) { ") # Unclosed brace
    run_case(12, "Parser", "Unbalanced Braces", f"submit {f_err2}", "Syntax Error", "PID: 1")
    
    f_err3 = create_file("err_lex.lang", "var x = @;") # Bad char
    run_case(13, "Parser", "Lexical Error", f"submit {f_err3}", "Unknown character", "PID: 1")
    
    run_case(14, "Shell", "Submit Missing File", "submit ghost.lang", "File 'ghost.lang' not found", "PID:")
    
    f_empty = create_file("empty.lang", "")
    run_case(15, "Parser", "Empty File", f"submit {f_empty}", "Compilation failed", "PID: 1")

    # =========================================================================
    # CATEGORY 4: VM EXECUTION (CORRECTNESS)
    # =========================================================================
    # Re-use valid files generated above
    run_case(16, "VM", "Run Valid PID 1", f"submit {f_valid}\nrun 1", "[VM] Halted")
    
    f_logic = create_file("logic.lang", "var x = 10; var y = 20; if (x > y) { x=0; } else { x=1; }")
    run_case(17, "VM", "If/Else Logic", f"submit {f_logic}\nrun 1", "[VM] Halted")
    
    f_fib = create_file("fib.lang", "var a=0; var b=1; var i=0; while(i<5){ var t=a+b; a=b; b=t; i=i+1; }")
    run_case(18, "VM", "Fibonacci Loop Execution", f"submit {f_fib}\nrun 1", "[VM] Halted")
    
    run_case(19, "VM", "Run Non-Existent PID", "run 99", "Invalid PID")
    
    run_case(20, "VM", "Run Finished PID", f"submit {f_valid}\nrun 1\nrun 1", "already finished")

    # =========================================================================
    # CATEGORY 5: VM RUNTIME ERRORS (ROBUSTNESS)
    # =========================================================================
    f_div0 = create_file("div0.lang", "var x = 10 / 0;")
    run_case(21, "VM", "Division by Zero", f"submit {f_div0}\nrun 1", "DivZero")
    
    # =========================================================================
    # CATEGORY 6: DEBUGGER (CONTROL FLOW)
    # =========================================================================
    f_debug = create_file("debug.lang", "var x = 1; \nvar y = 2; \nvar z = 3;")
    
    # Test Step
    run_case(22, "Debug", "Step Command", 
             f"submit {f_debug}\ndebug 1\nstep\nquit", 
             "Executed 1 instr")
    
    # Test Breakpoint (Valid)
    run_case(23, "Debug", "Set Valid Breakpoint", 
             f"submit {f_debug}\ndebug 1\nbreak 2\nquit", 
             "Breakpoint SET")
    
    # Test Continue to Breakpoint
    run_case(24, "Debug", "Continue to Breakpoint", 
             f"submit {f_debug}\ndebug 1\nbreak 2\ncontinue\nquit", 
             "Hit Breakpoint")
    
    # Test Regs
    run_case(25, "Debug", "Inspect Registers", 
             f"submit {f_debug}\ndebug 1\nstep\nregs\nquit", 
             "PC:")

    # =========================================================================
    # CATEGORY 7: DEBUGGER (ROBUSTNESS)
    # =========================================================================
    run_case(26, "Debug", "Debug Invalid PID", "debug 99", "PID not found")
    
    run_case(27, "Debug", "Break Invalid Line", 
             f"submit {f_debug}\ndebug 1\nbreak 99\nquit", 
             "No instruction found")
    
    run_case(28, "Debug", "Break Line 0", 
             f"submit {f_debug}\ndebug 1\nbreak 0\nquit", 
             "Usage: break")
             
    run_case(29, "Debug", "Unknown Command", 
             f"submit {f_debug}\ndebug 1\ndance\nquit", 
             "Unknown command")
             
    run_case(30, "Debug", "Quit functionality", 
             f"submit {f_debug}\ndebug 1\nquit", 
             "myshell>")

    # =========================================================================
    # CATEGORY 8: GARBAGE COLLECTOR (BASICS)
    # =========================================================================
    run_case(31, "GC", "Memstat Query", f"submit {f_valid}\nmemstat 1", "Active Objects")
    
    f_gc = create_file("gc.lang", "var x = 10;")
    run_case(32, "GC", "Implicit Allocation", f"submit {f_gc}\nrun 1", "Halted") # Should allow small allocs
    
    # =========================================================================
    # CATEGORY 9: GARBAGE COLLECTOR (STRESS & CYCLES)
    # =========================================================================
    f_stress = create_file("stress.lang", "var i=0; while(i<100) { var x=i*2; i=i+1; }")
    run_case(33, "GC", "Stress Allocation Loop", 
             f"submit {f_stress}\nrun 1", 
             "[GC] Triggered") # Should trigger GC at least once
             
    run_case(34, "GC", "GC Reclaiming Memory", 
             f"submit {f_stress}\nrun 1", 
             "Reclaimed:") # Check logs for reclamation
             
    # =========================================================================
    # CATEGORY 10: INTEGRATION MARATHON (MIXED)
    # =========================================================================
    # 35. Submit multiple files
    run_case(35, "Integ", "Multi-Process Submission", 
             f"submit {f_valid}\nsubmit {f_math}\nmemstat 1\nmemstat 2", 
             "PID: 2")
             
    # 36. Run multiple processes sequentially
    run_case(36, "Integ", "Sequential Execution", 
             f"submit {f_valid}\nsubmit {f_math}\nrun 1\nrun 2", 
             "Halted")

    # 37. Interleaved Debugging
    # Note: We can't actually interleave in this linear script easily, but we simulate switching
    run_case(37, "Integ", "Debug PID 2", 
             f"submit {f_valid}\nsubmit {f_math}\ndebug 2\nstep\nquit", 
             "Debugger attached to PID 2")

    # 38. Shell robustness on garbage input
    run_case(38, "Shell", "Garbage Input Robustness", "sdlfkjsdflkjsdf", "command not found")
    
    # 39. Empty lines resilience
    run_case(39, "Shell", "Many Empty Lines", "\n\n\n\nls", "Makefile")
    
    # 40. Compiler Robustness on huge file
    huge_code = "var x = 1;\n" * 100
    f_huge = create_file("huge.lang", huge_code)
    run_case(40, "Parser", "Compile Large File", f"submit {f_huge}", "PID: 1")
    
    # 41. VM Stack Overflow Check (Simulated by recursion if we had functions, 
    # but here tested by filling stack)
    # (Optional: requires deeper code support, checking basic stability here)
    run_case(41, "VM", "Stability Check", f"submit {f_huge}\nrun 1", "Halted")

    # 42-50: Edge Cases & Argument Variations
    run_case(42, "Shell", "Submit with extra args", f"submit {f_valid} extra_arg", "PID: 1") # Should ignore or handle
    run_case(43, "Debug", "Step without running", f"submit {f_valid}\ndebug 1\nregs\nquit", "PC: 0")
    run_case(44, "GC", "GC on fresh process", f"submit {f_valid}\nmemstat 1", "Active Objects: 0")
    run_case(45, "VM", "Run PID 0", "run 0", "Invalid PID")
    run_case(46, "VM", "Run Negative PID", "run -1", "Invalid PID")
    run_case(47, "Shell", "Complex args", "echo \"foo bar\"", "foo bar")
    run_case(48, "Shell", "Pipes (OS)", "ls | wc", "") # Checking it doesn't crash
    run_case(49, "Shell", "Background (OS)", "sleep 1 &", "") # Checking it doesn't crash
    run_case(50, "System", "Full Cycle", f"submit {f_fib}\ndebug 1\nbreak 5\ncontinue\nregs\nrun\nmemstat 1\nexit", "Exiting")

    # --- SUMMARY ---
    print("\n" + "="*40)
    print(f"       TEST SUITE COMPLETION REPORT")
    print("="*40)
    
    passed_count = sum(1 for r in results if r['status'] == 'PASS')
    failed_count = sum(1 for r in results if r['status'] == 'FAIL')
    
    print(f"Total Tests: {len(results)}")
    print(f"Passed:      {GREEN}{passed_count}{RESET}")
    print(f"Failed:      {RED if failed_count > 0 else GREEN}{failed_count}{RESET}")
    
    if failed_count > 0:
        print(f"\n{RED}FAILURES DETECTED:{RESET}")
        for r in results:
            if r['status'] == 'FAIL':
                print(f" - Test {r['id']} ({r['desc']}): {r.get('reason')}")
        print(f"\nCheck {LOG_FILE} for detailed debugging.")
    else:
        print(f"\n{GREEN}ALL 50 TESTS PASSED. SYSTEM IS ROBUST.{RESET}")

if __name__ == "__main__":
    main()