import os
import random
import subprocess
import shutil
import sys
import time

# --- CONFIGURATION ---
SYSTEM_BIN = "./nizsystem"
TEST_DIR = "./automated_tests/stress_tests"
VALID_DIR = os.path.join(TEST_DIR, "valid")
INVALID_DIR = os.path.join(TEST_DIR, "invalid")
LOG_FILE = "./automated_tests/test_results.log"

# Colors for Output
GREEN = "\033[92m"
RED = "\033[91m"
BLUE = "\033[94m"
RESET = "\033[0m"

# --- GENERATORS ---

def generate_valid_program(i):
    """Generates a grammatically correct program."""
    templates = [
        # 1. Simple Math
        f"var a = {random.randint(1, 100)};\nvar b = {random.randint(1, 100)};\nvar c = a + b * 2;",
        # 2. If/Else Logic
        f"var x = {random.randint(1, 50)};\nif (x > 25) {{ x = x - 1; }} else {{ x = x + 1; }}",
        # 3. While Loop
        f"var i = 0;\nwhile (i < {random.randint(1, 5)}) {{ i = i + 1; }}",
        # 4. Nested Scopes
        f"var g = 10;\n{{ var l = 20; g = g + l; }}",
        # 5. Complex Expression
        f"var res = (10 + 20) * 3 / 2 - 5;"
    ]
    return random.choice(templates)

def generate_invalid_program(i):
    """Generates a program with specific syntax errors."""
    error_types = [
        # 1. Missing Semicolon
        f"var x = {i} \n var y = 2;",
        # 2. Unbalanced Braces
        f"if (x > 0) {{ x = x + 1; ",
        # 3. Invalid Operator
        f"var x = 10 @ 20;",
        # 4. Use of reserved keyword as ID
        f"var if = 10;",
        # 5. Missing parentheses
        f"if x > 0 {{ }}",
        # 6. Garbage
        "This is not code."
    ]
    return random.choice(error_types)

def setup_directories():
    if os.path.exists(TEST_DIR): shutil.rmtree(TEST_DIR)
    os.makedirs(VALID_DIR)
    os.makedirs(INVALID_DIR)

def generate_files():
    print(f"{BLUE}[INFO] Generating 100 Valid and 100 Invalid Test Cases...{RESET}")
    for i in range(1, 101):
        # Valid
        with open(f"{VALID_DIR}/test_{i}.lang", "w") as f:
            f.write(generate_valid_program(i))
        # Invalid
        with open(f"{INVALID_DIR}/fail_{i}.lang", "w") as f:
            f.write(generate_invalid_program(i))
    print(f"{GREEN}[SUCCESS] Generation Complete.{RESET}\n")

# --- EXECUTION ENGINE ---

def run_shell_command(input_str):
    """Feeds input to nizsystem and captures output."""
    try:
        process = subprocess.Popen(
            [SYSTEM_BIN],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        stdout, stderr = process.communicate(input=input_str, timeout=1)
        return stdout + stderr
    except subprocess.TimeoutExpired:
        process.kill()
        return "TIMEOUT"
    except FileNotFoundError:
        print(f"{RED}[FATAL] Could not find {SYSTEM_BIN}. Did you compile?{RESET}")
        sys.exit(1)

def verify_valid_tests():
    print(f"{BLUE}--- RUNNING 100 POSITIVE TESTS (Must Compile & Run) ---{RESET}")
    passed = 0
    for i in range(1, 101):
        filename = f"{VALID_DIR}/test_{i}.lang"
        # We submit, then try to run PID 1 (since shell cleans up, pid 1 is usually reused or we map correctly)
        # Note: In your shell, PID increments. We need to parse PID or just assume sequential for stress test.
        # Strategy: submit, checks for "Process submitted".
        
        commands = f"submit {filename}\nexit\n"
        output = run_shell_command(commands)
        
        if "Process submitted" in output and "Syntax Error" not in output:
            passed += 1
            # print(f"Valid Test {i}: {GREEN}PASS{RESET}")
        else:
            print(f"Valid Test {i}: {RED}FAIL{RESET}")
            # print(f"Output: {output}")

    print(f"Result: {passed}/100 Valid Tests Passed.\n")
    return passed

def verify_invalid_tests():
    print(f"{BLUE}--- RUNNING 100 NEGATIVE TESTS (Must Fail Gracefully) ---{RESET}")
    passed = 0
    for i in range(1, 101):
        filename = f"{INVALID_DIR}/fail_{i}.lang"
        commands = f"submit {filename}\nexit\n"
        output = run_shell_command(commands)
        
        # We EXPECT "Syntax Error" or "Compile Failed"
        # We do NOT want crashes (Segfaults) or "Process submitted"
        
        if ("Syntax Error" in output or "Compile Failed" in output) and "Segmentation fault" not in output:
            passed += 1
        else:
            print(f"Invalid Test {i}: {RED}FAIL (System accepted bad code or Crashed){RESET}")
            # print(f"Output: {output}")

    print(f"Result: {passed}/100 Invalid Tests Passed.\n")
    return passed

def verify_shell_features():
    print(f"{BLUE}--- VERIFYING SHELL FEATURES (Pipelines, Background) ---{RESET}")
    
    # 1. Pipeline Test
    out_pipe = run_shell_command("echo hello | wc -c\nexit\n")
    if "6" in out_pipe: 
        print(f"Pipeline Test: {GREEN}PASS{RESET}")
    else: 
        print(f"Pipeline Test: {RED}FAIL{RESET}")

    # 2. Redirection Test
    test_file = f"{TEST_DIR}/redir_test.txt"
    run_shell_command(f"echo 999 > {test_file}\nexit\n")
    if os.path.exists(test_file) and "999" in open(test_file).read():
         print(f"Redirection Test: {GREEN}PASS{RESET}")
    else:
         print(f"Redirection Test: {RED}FAIL{RESET}")

    # 3. Background Test
    out_bg = run_shell_command("sleep 1 &\nexit\n")
    if "[BG PID" in out_bg or "PID" in out_bg:
        print(f"Background Test: {GREEN}PASS{RESET}")
    else:
        print(f"Background Test: {RED}FAIL{RESET}")

def main():
    if not os.path.exists(SYSTEM_BIN):
        print(f"{RED}[ERROR] Binary {SYSTEM_BIN} not found. Build it first!{RESET}")
        return

    setup_directories()
    generate_files()
    
    v_score = verify_valid_tests()
    i_score = verify_invalid_tests()
    verify_shell_features()
    
    total = v_score + i_score
    print(f"{BLUE}=== FINAL SCORE: {total}/200 ==={RESET}")
    if total == 200:
        print(f"{GREEN}PERFECT ROBUSTNESS ACHIEVED.{RESET}")
    else:
        print(f"{RED}Check failures above.{RESET}")

if __name__ == "__main__":
    main()