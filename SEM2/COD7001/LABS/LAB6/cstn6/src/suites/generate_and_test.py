import os
import subprocess
import shutil
import sys

# --- CONFIG ---
SYSTEM_BIN = "./nizsystem"
TEST_DIR = "tests_final"
VALID_DIR = os.path.join(TEST_DIR, "valid")
INVALID_DIR = os.path.join(TEST_DIR, "invalid")

# --- COLORS ---
GREEN = "\033[92m"
RED = "\033[91m"
BLUE = "\033[94m"
RESET = "\033[0m"

# --- 10 NEW VALID PROGRAMS (Logic & Algorithms) ---
valid_programs = {
    # 1. Power Calculation (2^5)
    "v01_power.lang": """
        var base = 2;
        var exp = 5;
        var result = 1;
        while (exp > 0) {
            result = result * base;
            exp = exp - 1;
        }
    """,

    # 2. Maximum of 3 Numbers
    "v02_max_of_three.lang": """
        var a = 50;
        var b = 90;
        var c = 20;
        var max = 0;
        if (a > b) { max = a; } else { max = b; }
        if (c > max) { max = c; }
    """,

    # 3. Integer Division (Manual Subtraction)
    "v03_custom_div.lang": """
        var num = 100;
        var den = 7;
        var count = 0;
        while (num > den) {
            num = num - den;
            count = count + 1;
        }
        // count is the quotient, num is the remainder
    """,

    # 4. Sum of Squares (1^2 + 2^2 + ... + 5^2)
    "v04_sum_squares.lang": """
        var i = 1;
        var limit = 5;
        var sum = 0;
        while (i < limit + 1) {
            var sq = i * i;
            sum = sum + sq;
            i = i + 1;
        }
    """,

    # 5. Swap Variables (Arithmetic)
    "v05_swap.lang": """
        var a = 10;
        var b = 20;
        a = a + b; // 30
        b = a - b; // 10
        a = a - b; // 20
    """,

    # 6. Factorial with separate scope
    "v06_scope_fact.lang": """
        var res = 1;
        {
            var n = 6;
            while (n > 1) {
                res = res * n;
                n = n - 1;
            }
        }
        // n is gone, res remains
    """,

    # 7. Even/Odd Checker (using 2)
    "v07_is_even.lang": """
        var n = 15;
        while (n > 1) {
            n = n - 2;
        }
        var is_even = 0;
        if (n == 0) { is_even = 1; }
    """,

    # 8. Complex Precedence check
    "v08_precedence.lang": """
        var x = 10;
        var y = 5;
        var z = 2;
        var res = x + y * z - (x / z); 
        // 10 + 10 - 5 = 15
    """,

    # 9. Empty Body Loop (Valid Syntax)
    "v09_empty_loop.lang": """
        var i = 10;
        while (i < 0) { 
            // Should never run
            i = i + 1; 
        }
    """,

    # 10. Large Constants
    "v10_large_nums.lang": """
        var big = 1000;
        var huge = big * 100;
        var small = huge / 10;
    """
}

# --- 10 NEW INVALID PROGRAMS (Syntax Traps) ---
invalid_programs = {
    # 1. Keyword as Variable
    "i01_keyword_var.lang": "var while = 100;",

    # 2. Missing Operand
    "i02_trailing_op.lang": "var x = 10 + ;",

    # 3. Double Semicolon (Empty Statement not supported in your grammar)
    "i03_double_semi.lang": "var x = 1;;",

    # 4. Unknown Character
    "i04_bad_char.lang": "var price = $10;",

    # 5. Unbalanced Braces
    "i05_open_brace.lang": "if (x > 0) { x = 1;",

    # 6. Unbalanced Parentheses
    "i06_open_paren.lang": "var x = (10 + 5 * 2;",

    # 7. Assignment to Number
    "i07_assign_num.lang": "10 = x;",

    # 8. Missing 'var' keyword
    "i08_no_var.lang": "x = 10;", # Unless defined before, this might pass if logic allows implicit global, but your grammar requires decl first.

    # 9. Malformed If
    "i09_malformed_if.lang": "if x > 10 { }", # Missing parens

    # 10. Bad Binary File
    "i10_garbage.bin": "\x7F\x45\x4C\x46\x01\x01" # ELF Header (Should fail parse)
}

def generate():
    if os.path.exists(TEST_DIR): shutil.rmtree(TEST_DIR)
    os.makedirs(VALID_DIR)
    os.makedirs(INVALID_DIR)
    
    print(f"{BLUE}[GEN] Creating Test Files...{RESET}")
    for name, code in valid_programs.items():
        with open(os.path.join(VALID_DIR, name), "w") as f: f.write(code)
    for name, code in invalid_programs.items():
        with open(os.path.join(INVALID_DIR, name), "w") as f: f.write(code)
    print(f"{GREEN}[GEN] Done.{RESET}\n")

def run_test(name, filepath, expect_success):
    # We submit, check for AST, and then try to run (if valid)
    cmd_str = f"submit {filepath}\n"
    if expect_success:
        cmd_str += "run 1\n" 
    
    full_cmd = cmd_str + "exit\n"
    
    try:
        proc = subprocess.Popen(
            [SYSTEM_BIN], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )
        stdout, stderr = proc.communicate(input=full_cmd, timeout=2)
        output = stdout + stderr
        
        # ANALYSIS
        passed = False
        msg = ""

        if expect_success:
            if "Process submitted" in output and "--- AST ---" in output:
                if "Syntax Error" not in output:
                    passed = True
                else: msg = "Syntax Error in valid file"
            else: msg = "Failed to submit or print AST"
        else:
            if "Syntax Error" in output or "Compile Failed" in output or "Unknown char" in output:
                if "Segmentation fault" not in output:
                    passed = True
                else: msg = "CRASHED (Segfault)"
            else: msg = "Accepted invalid code"

        # Special check for Binary file
        if "bin" in name:
            if "Unknown char" in output or "Syntax Error" in output: passed = True

        status = f"{GREEN}PASS{RESET}" if passed else f"{RED}FAIL{RESET}"
        print(f"Test {name:<25}: {status} {msg}")
        return 1 if passed else 0

    except Exception as e:
        print(f"Test {name:<25}: {RED}ERROR{RESET} {e}")
        return 0

def main():
    if not os.path.exists(SYSTEM_BIN):
        print("Binary not found. Make sure to compile first!")
        return

    generate()
    
    score = 0
    total = 0
    
    print(f"{BLUE}--- VALID PROGRAMS (Must print AST & Submit) ---{RESET}")
    for name in sorted(valid_programs.keys()):
        score += run_test(name, os.path.join(VALID_DIR, name), True)
        total += 1
        
    print(f"\n{BLUE}--- INVALID PROGRAMS (Must Fail Gracefully) ---{RESET}")
    for name in sorted(invalid_programs.keys()):
        score += run_test(name, os.path.join(INVALID_DIR, name), False)
        total += 1
        
    print(f"\n{BLUE}=== FINAL SCORE: {score}/{total} ==={RESET}")

if __name__ == "__main__":
    main()