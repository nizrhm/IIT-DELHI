import subprocess
import os
import sys

# Configuration
SHELL_PATH = "./mysystem"
TEST_DIR = "automated_tests"

# Colors
GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
RESET = "\033[0m"

def setup():
    if not os.path.exists(TEST_DIR):
        os.makedirs(TEST_DIR)
    
    # 1. Valid Program
    with open(f"{TEST_DIR}/valid.lang", "w") as f:
        f.write("var x = 10; var y = 20; if (x < y) { x = x + 5; } else { x = 0; }")

    # 2. Syntax Error
    with open(f"{TEST_DIR}/syntax_err.lang", "w") as f:
        f.write("var x = 10 \n if (x < 5) { }") 

    # 3. GC Stress
    with open(f"{TEST_DIR}/gc_test.lang", "w") as f:
        f.write("var i = 0; while (i < 100) { var temp = 50; i = i + 1; }")

def run_test_case(name, inputs, expected_patterns, unexpected_patterns=[]):
    print(f"{YELLOW}Running Test: {name}...{RESET}", end=" ")
    
    process = subprocess.Popen(
        [SHELL_PATH],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    input_str = "\n".join(inputs) + "\nexit\n"
    
    try:
        stdout, stderr = process.communicate(input=input_str, timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        print(f"{RED}[FAIL] (Timeout){RESET}")
        return False

    # Normalize output: remove \r for Windows/WSL compatibility
    full_output = (stdout + stderr).replace("\r", "")
    
    missing = []
    for pattern in expected_patterns:
        if pattern not in full_output:
            missing.append(pattern)
    
    errors = []
    for pattern in unexpected_patterns:
        if pattern in full_output:
            errors.append(pattern)

    if not missing and not errors:
        print(f"{GREEN}[PASS]{RESET}")
        return True
    else:
        print(f"{RED}[FAIL]{RESET}")
        print("   -> Reason:", end=" ")
        if missing: print(f"Missing output: {missing}")
        if errors: print(f"Found error: {errors}")
        
        # DEBUG: Print exact representation to catch hidden chars
        print(f"   -> Debug (Repr): {repr(full_output[:100])}...") 
        print(f"   -> Full Output Snippet:\n{full_output[:300]}...\n")
        return False

def main():
    if not os.path.exists(SHELL_PATH):
        print(f"{RED}Error: Executable {SHELL_PATH} not found. Run 'make' in 'src/' first.{RESET}")
        sys.exit(1)

    setup()
    print("--- Starting Integrated System Tests (Robust) ---\n")
    
    passed = 0
    total = 0

    # TEST 1: Submission
    total += 1
    if run_test_case("Lab 1 & 3: Valid Submission", 
                     [f"submit {TEST_DIR}/valid.lang"], 
                     ["Process submitted", "PID: 1"]): passed += 1

    # TEST 2: VM Execution (The one that failed previously)
    total += 1
    if run_test_case("Lab 4: VM Execution", 
                     [f"submit {TEST_DIR}/valid.lang", "run 1"], 
                     ["Starting execution", "VM] Halted", "PID: 1"]): passed += 1

    # TEST 3: Syntax Error
    total += 1
    if run_test_case("Lab 3: Syntax Error Handling", 
                     [f"submit {TEST_DIR}/syntax_err.lang"], 
                     ["Syntax Error"], ["PID: 1"]): passed += 1

    # TEST 4: GC
    total += 1
    if run_test_case("Lab 5: Memory & GC", 
                     [f"submit {TEST_DIR}/gc_test.lang", "memstat 1"], 
                     ["Memory Report", "Active Objects"]): passed += 1
    
    # TEST 5: Debugger
    total += 1
    if run_test_case("Lab 2: Debugger Stepping", 
                     [f"submit {TEST_DIR}/valid.lang", "debug 1", "step", "quit"], 
                     ["(debug)"]): passed += 1

    # TEST 6: Robustness
    total += 1
    if run_test_case("System Robustness (Bad PID)", 
                     ["run 99"], 
                     ["Error: Invalid PID"]): passed += 1

    print("\n----------------------------------------")
    print(f"Summary: {passed}/{total} Tests Passed")
    if passed == total:
        print(f"{GREEN}ALL SYSTEMS NOMINAL. READY FOR DEMO.{RESET}")
    else:
        print(f"{RED}FAILURE DETECTED.{RESET}")

if __name__ == "__main__":
    main()