import os
import subprocess
import shutil

SYSTEM_BIN = "./nizsystem"
TEST_DIR = "stress_tests"
if os.path.exists(TEST_DIR): shutil.rmtree(TEST_DIR)
os.makedirs(TEST_DIR)

GREEN = "\033[92m"
RED = "\033[91m"
RESET = "\033[0m"

def create_file(name, content):
    path = os.path.join(TEST_DIR, name)
    with open(path, "w") as f: f.write(content)
    return path

def run_shell(commands):
    full_cmd = commands + "\nexit\n"
    try:
        proc = subprocess.Popen([SYSTEM_BIN], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        stdout, stderr = proc.communicate(input=full_cmd, timeout=5)
        return stdout + stderr
    except Exception as e:
        return str(e)

print(f"{GREEN}=== STARTING ROBUSTNESS STRESS TEST ==={RESET}")

# 1. TEST DEEP NESTING (Parser Limits)
print("1. Testing Deep Nesting (100 levels)...", end=" ")
nested_code = "var x = 0;" + ("{" * 100) + "x = x + 1;" + ("}" * 100)
f1 = create_file("deep.lang", nested_code)
out = run_shell(f"submit {f1}")
if "Process submitted" in out and "Syntax Error" not in out: print(f"{GREEN}PASS{RESET}")
else: print(f"{RED}FAIL{RESET} (Parser crashed or rejected nesting)")

# 2. TEST GC PRESSURE (Memory Limits)
print("2. Testing GC Pressure (10,000 allocations)...", end=" ")
gc_code = """
var i = 0;
while (i < 10000) {
    var temp = i * 2; // New object every time
    i = i + 1;
}
"""
f2 = create_file("heavy_gc.lang", gc_code)
# We run it and check if it finishes without crashing
out = run_shell(f"submit {f2}\nrun 1")
if "[VM] Halted" in out or "Process Terminated" in out: 
    print(f"{GREEN}PASS{RESET}")
else: print(f"{RED}FAIL{RESET} (VM crashed under load)")

# 3. TEST COMPILER STATE RESET (The "Line 5" Bug)
print("3. Testing Compiler State Reset...", end=" ")
bad_code = "var x = ;" # Error line 1
good_code = "var x = 10;"
f3_bad = create_file("reset_bad.lang", bad_code)
f4_good = create_file("reset_good.lang", good_code)

out = run_shell(f"submit {f3_bad}\nsubmit {f4_good}\nsubmit {f3_bad}")
# We expect "Syntax Error line 1" TWICE. If it says line 2 or 5, fail.
err_count = out.count("Syntax Error line 1")
if err_count == 2 and "Process submitted" in out: print(f"{GREEN}PASS{RESET}")
else: print(f"{RED}FAIL{RESET} (Line numbers did not reset)")

# 4. TEST STRICT VARIABLE DECLARATION (The "Implicit Global" Bug)
print("4. Testing Strict Variable Mode...", end=" ")
undeclared_code = "x = 10;" # Should fail
f5 = create_file("undeclared.lang", undeclared_code)
out = run_shell(f"submit {f5}")
if "Compile Error" in out and "Undeclared variable" in out: print(f"{GREEN}PASS{RESET}")
else: print(f"{RED}FAIL{RESET} (Allowed undeclared variable)")

print(f"\n{GREEN}=== STRESS TEST COMPLETE ==={RESET}")