import subprocess
import os
import sys
import shutil

SHELL_BIN = "./mysystem"
TEST_DIR = "./tests/tests_final"
LOG_FILE = "./tests/verification_report.log"
GREEN, RED, RESET = "\033[92m", "\033[91m", "\033[0m"

def setup():
    if os.path.exists(TEST_DIR): shutil.rmtree(TEST_DIR)
    os.makedirs(TEST_DIR)

def create_file(name, content):
    path = os.path.join(TEST_DIR, name)
    with open(path, "w") as f: f.write(content)
    return path

def run_test(id, name, commands, expected):
    print(f"Test {id:02d}: {name:<30}", end="")
    try:
        proc = subprocess.Popen([SHELL_BIN], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        # Add exit to ensure buffer flush
        inp = "\n".join(commands) + "\nexit\n"
        out, err = proc.communicate(input=inp, timeout=2)
        full_out = out + err
    except subprocess.TimeoutExpired:
        proc.kill()
        full_out = "[TIMEOUT]"

    if expected in full_out:
        print(f"{GREEN}[PASS]{RESET}")
        return True
    else:
        print(f"{RED}[FAIL]{RESET}")
        with open(LOG_FILE, "a") as f:
            f.write(f"\n[TEST {id} FAILED]\nEXPECTED: {expected}\nOUTPUT:\n{full_out}\n")
        return False

def main():
    setup()
    with open(LOG_FILE, "w") as f: f.write("FINAL VERIFICATION\n")

    # 1. CORE
    run_test(1, "OS Command", ["ls"], "Makefile")
    
    # 2. PARSER (FIXED MATH)
    f_math = create_file("math.lang", "var x = (5 + 5) * 2;")
    run_test(2, "Math (Parens)", [f"submit {f_math}", "run 1"], "[VM] Halted")

    # 3. DEBUGGER (FIXED LINE NUMBERS)
    # Note: We use \n explicitly to create lines 1, 2, 3
    f_debug = create_file("debug.lang", "var a=1;\nvar b=2;\nvar c=3;")
    run_test(3, "Breakpoints", [f"submit {f_debug}", "debug 1", "break 2", "quit"], "Breakpoint SET")
    run_test(4, "Continue", [f"submit {f_debug}", "debug 1", "break 2", "continue", "quit"], "Hit Breakpoint")

    # 4. GC
    f_gc = create_file("gc.lang", "var i=0; while(i<50) { var t=i; i=i+1; }")
    run_test(5, "GC Trigger", [f"submit {f_gc}", "run 1"], "[GC] Reclaimed")

if __name__ == "__main__":
    main()