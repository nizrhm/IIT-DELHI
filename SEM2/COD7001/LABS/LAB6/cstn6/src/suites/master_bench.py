import os
import subprocess
import shutil
import time
import sys
import random

# --- CONFIGURATION ---
SYSTEM_BIN = "./nizsystem"
TEST_DIR = "master_tests"
VALID_DIR = os.path.join(TEST_DIR, "valid")
INVALID_DIR = os.path.join(TEST_DIR, "invalid")
LOG_FILE = "master_bench.log"

GREEN = "\033[92m"
RED = "\033[91m"
BLUE = "\033[94m"
RESET = "\033[0m"

class TestBench:
    def __init__(self):
        self.score = 0
        self.total = 0
        self.setup()

    def setup(self):
        if os.path.exists(TEST_DIR): shutil.rmtree(TEST_DIR)
        os.makedirs(VALID_DIR)
        os.makedirs(INVALID_DIR)
        if not os.path.exists(SYSTEM_BIN):
            print(f"{RED}[FATAL] {SYSTEM_BIN} not found. Run 'make' first.{RESET}")
            sys.exit(1)

    def log(self, msg):
        print(msg)

    def run_shell(self, input_cmds):
        try:
            full_input = input_cmds + "\nexit\n"
            proc = subprocess.Popen(
                [SYSTEM_BIN],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            stdout, stderr = proc.communicate(input=full_input, timeout=3)
            return stdout + stderr
        except subprocess.TimeoutExpired:
            proc.kill()
            return "TIMEOUT"

    def expect(self, name, input_cmd, expected_str, forbidden_str=None):
        self.total += 1
        output = self.run_shell(input_cmd)
        passed = True
        
        if expected_str not in output:
            passed = False
            fail_reason = f"Missing '{expected_str}'"
        
        if forbidden_str and forbidden_str in output:
            passed = False
            fail_reason = f"Found forbidden '{forbidden_str}'"
            
        if passed:
            self.score += 1
            print(f"{name:<50} {GREEN}[PASS]{RESET}")
        else:
            print(f"{name:<50} {RED}[FAIL]{RESET} -> {fail_reason}")

    # --- TESTS ---
    def test_lab1_functional(self):
        self.log(f"\n{BLUE}--- LAB 1: SHELL FUNCTIONAL ---{RESET}")
        self.expect("L1: Quoted Arguments", 'echo "hello world"', "hello world")
        self.expect("L1: Pipeline (|)", "echo hello | wc -c", "6")
        f = f"{TEST_DIR}/out.txt"
        self.expect("L1: Redirection (>)", f"echo 123 > {f}", "")
        if os.path.exists(f) and "123" in open(f).read():
            self.score += 1; print(f"{'L1: File Verification':<50} {GREEN}[PASS]{RESET}")
        else: print(f"{'L1: File Verification':<50} {RED}[FAIL]{RESET}")
        self.total += 1

    def test_lab1_nonfunctional(self):
        self.log(f"\n{BLUE}--- LAB 1: ROBUSTNESS ---{RESET}")
        self.expect("L1: Malformed Pipe (ls |)", "ls |", "Syntax Error", "Segmentation fault")
        self.expect("L1: Missing File (<)", "cat < ghost.txt", "No such file")

    def test_lab1_optional(self):
        self.log(f"\n{BLUE}--- LAB 1: OPTIONAL ---{RESET}")
        self.expect("L1: Built-in CD", "cd ..\npwd", os.path.dirname(os.getcwd()))
        self.expect("L1: History", "echo test_hist\nhistory", "test_hist")

    def generate_suite(self):
        self.log(f"\n{BLUE}--- LAB 3: COMPILER ---{RESET}")
        for i in range(1, 20):
            with open(f"{VALID_DIR}/ok_{i}.lang", "w") as f:
                f.write(f"var a={i}; var b=a*2; if(b>{i}) {{ b=b-1; }}")
            with open(f"{INVALID_DIR}/bad_{i}.lang", "w") as f:
                f.write(f"var x = {i} var y = 10;") 

        self.expect("L3: Compile Valid", f"submit {VALID_DIR}/ok_1.lang", "Process submitted")
        self.expect("L3: Compile Invalid", f"submit {INVALID_DIR}/bad_1.lang", "Syntax Error", "Process submitted")

    def test_lab4_vm(self): 
        self.log(f"\n{BLUE}--- LAB 4: VM SAFETY ---{RESET}")
        code_math = f"{TEST_DIR}/math.lang"
        with open(code_math, "w") as f: f.write("var x = 10; var y = 20; var z = (x + y) * 2;") 
        self.expect("L4: Submit Math Program", f"submit {code_math}", "PID")
        
        # Test 2: DivZero (PID 1)
        code_div0 = f"{TEST_DIR}/div0.lang"
        with open(code_div0, "w") as f: f.write("var a = 10 / 0;")
        self.expect("L4: Safety (Div By Zero)", f"submit {code_div0}\nrun 1", "DivZero") 

    def test_lab5_gc(self):
        self.log(f"\n{BLUE}--- LAB 5: GARBAGE COLLECTOR ---{RESET}")
        code_gc = f"{TEST_DIR}/gc.lang"
        with open(code_gc, "w") as f:
            f.write("var i = 0; while(i < 50) { var temp = i * 2; i = i + 1; }")
        
        self.expect("L5: Submit GC Stress", f"submit {code_gc}", "PID")
        # Check for existence of output, not 0 (since VM allocates INTs)
        self.expect("L5: Memstat (Pre-Run)", f"submit {code_gc}\nmemstat 1", "Heap Objects:")
        self.expect("L5: Run GC Program", f"submit {code_gc}\nrun 1", "")
        self.expect("L5: Leaks Command", f"submit {code_gc}\nleaks 1", "Leak Check Passed")

    def test_integration(self):
        self.log(f"\n{BLUE}--- INTEGRATION ---{RESET}")
        # Debug PID 1
        debug_session = f"submit {TEST_DIR}/math.lang\ndebug 1\nbreak 0\nrun\nstack\nstep\nquit\n"
        out = self.run_shell(debug_session)
        
        if "[VM Debugger]" in out: self.score+=1; print(f"{'Debugger Attached':<50} {GREEN}[PASS]{RESET}")
        else: print(f"{'Debugger Attached':<50} {RED}[FAIL]{RESET}")
        self.total+=1
        
        if "Hit Breakpoint" in out or "Paused" in out: self.score+=1; print(f"{'Breakpoint Hit':<50} {GREEN}[PASS]{RESET}")
        else: print(f"{'Breakpoint Hit':<50} {RED}[FAIL]{RESET}")
        self.total+=1
        
        # Test Kill Command
        self.expect("Integration: Kill Process", f"submit {TEST_DIR}/math.lang\nkill 1", "killed")

    def run_all(self):
        self.test_lab1_functional()
        self.test_lab1_nonfunctional()
        self.test_lab1_optional()
        self.generate_suite()
        self.test_lab4_vm()
        self.test_lab5_gc()
        self.test_integration()
        print(f"\n{BLUE}=== MASTER BENCH RESULT: {self.score}/{self.total} ==={RESET}")

if __name__ == "__main__":
    TestBench().run_all()