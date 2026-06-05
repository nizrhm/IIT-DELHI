#!/bin/bash

# ==============================================================================
#  ROBUSTNESS TEST SUITE FOR INTEGRATED SYSTEM (LABS 1-6)
# ==============================================================================

SYSTEM_BIN="./mysystem"
TEST_DIR="tests"
LOG_FILE="./tests/test_results.log"

# Colors for Output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Initialize
rm -rf $TEST_DIR
mkdir -p $TEST_DIR
echo "" > $LOG_FILE

echo -e "${BLUE}======================================================${NC}"
echo -e "${BLUE}      STARTING ROBUSTNESS VALIDATION SUITE            ${NC}"
echo -e "${BLUE}======================================================${NC}"

if [ ! -f "$SYSTEM_BIN" ]; then
    echo -e "${RED}[CRITICAL] Executable $SYSTEM_BIN not found! Run 'make' in src/ first.${NC}"
    exit 1
fi

# ==============================================================================
#  HELPER FUNCTIONS
# ==============================================================================

# Function to run a test case
# Usage: run_test "Test Name" "Inputs" "Expected Pattern" "Unexpected Pattern (Optional)"
run_test() {
    local test_name="$1"
    local input_commands="$2"
    local expected="$3"
    local unexpected="$4"
    
    echo -e -n "Testing: ${YELLOW}$test_name${NC} ... "
    
    # Run system with timeout to prevent infinite loops from hanging the test
    # We pipe the input commands + 'exit' into the shell
    output=$(echo -e "$input_commands\nexit" | timeout 3s $SYSTEM_BIN 2>&1)
    exit_code=$?

    # Logging
    echo "---------------------------------------------------" >> $LOG_FILE
    echo "TEST: $test_name" >> $LOG_FILE
    echo "INPUT: $input_commands" >> $LOG_FILE
    echo "OUTPUT: $output" >> $LOG_FILE

    # Validation Logic
    if [ $exit_code -eq 124 ]; then
        echo -e "${RED}[FAIL] (Timeout/Hang)${NC}"
        return 1
    fi

    if [[ "$output" == *"$expected"* ]]; then
        if [ ! -z "$unexpected" ] && [[ "$output" == *"$unexpected"* ]]; then
             echo -e "${RED}[FAIL]${NC} (Found unexpected output: '$unexpected')"
             return 1
        fi
        echo -e "${GREEN}[PASS]${NC}"
        return 0
    else
        echo -e "${RED}[FAIL]${NC} (Missing expected output: '$expected')"
        return 1
    fi
}

# ==============================================================================
#  PHASE 1: GENERATE TEST FILES (Valid & Invalid)
# ==============================================================================

echo -e "\n${BLUE}[PHASE 1] Generating Test Cases...${NC}"

# 1. Valid: Simple Logic
cat > $TEST_DIR/valid_simple.lang <<EOF
var x = 10;
var y = 20;
if (x < y) { 
    x = x + 1; 
}
EOF

# 2. Valid: GC Stress (Loops + Allocation)
cat > $TEST_DIR/valid_gc_stress.lang <<EOF
var i = 0;
while (i < 50) {
    var a = 1;
    var b = 2;
    var c = 3;
    i = i + 1;
}
EOF

# 3. Invalid: Syntax Error (Missing Semicolon)
cat > $TEST_DIR/invalid_syntax.lang <<EOF
var x = 10
var y = 20;
EOF

# 4. Invalid: Lexical Error (Unknown Character)
cat > $TEST_DIR/invalid_lex.lang <<EOF
var x = 10;
var y = @; 
EOF

# ==============================================================================
#  PHASE 2: COMPILER & PARSER ROBUSTNESS (LAB 3)
# ==============================================================================
echo -e "\n${BLUE}[PHASE 2] Testing Lab 3 (Parser/Compiler) Safety${NC}"

# Test 1: Submit Valid File
run_test "Valid Submission" \
    "submit $TEST_DIR/valid_simple.lang" \
    "PID: 1" \
    "Syntax Error"

# Test 2: Submit Syntax Error File
run_test "Syntax Error Rejection" \
    "submit $TEST_DIR/invalid_syntax.lang" \
    "Syntax Error" \
    "PID:" # Should NOT assign a PID

# Test 3: Submit Non-Existent File
run_test "Missing File Handling" \
    "submit $TEST_DIR/ghost_file.lang" \
    "Error: File '$TEST_DIR/ghost_file.lang' not found" \
    "PID:"

# ==============================================================================
#  PHASE 3: EXECUTION & VM SAFETY (LAB 4)
# ==============================================================================
echo -e "\n${BLUE}[PHASE 3] Testing Lab 4 (VM) Execution Safety${NC}"

# Test 4: Run Valid PID
run_test "Execute Valid Program" \
    "submit $TEST_DIR/valid_simple.lang\nrun 1" \
    "[VM] Halted" \
    "Segmentation fault"

# Test 5: Run Invalid PID
run_test "Run Non-Existent PID" \
    "run 999" \
    "Error: Invalid PID" \
    "[VM] Halted"

# Test 6: Run Finished PID (Double Execution Prevention)
run_test "Prevent Re-running Finished PID" \
    "submit $TEST_DIR/valid_simple.lang\nrun 1\nrun 1" \
    "already finished" \
    "Segmentation fault"

# ==============================================================================
#  PHASE 4: MEMORY & GC (LAB 5)
# ==============================================================================
echo -e "\n${BLUE}[PHASE 4] Testing Lab 5 (Memory/GC) integration${NC}"

# Test 7: Memory Stats
run_test "Memory Statistics Query" \
    "submit $TEST_DIR/valid_simple.lang\nmemstat 1" \
    "Active Objects" \
    "Segmentation fault"

# Test 8: GC Stress Test (Ensure no crash on high allocation)
run_test "GC Stress Test (50 iterations)" \
    "submit $TEST_DIR/valid_gc_stress.lang\nrun 1" \
    "[VM] Halted" \
    "Stack Overflow"

# ==============================================================================
#  PHASE 5: DEBUGGER (LAB 2)
# ==============================================================================
echo -e "\n${BLUE}[PHASE 5] Testing Lab 2 (Debugger) integration${NC}"

# Test 9: Debugger Stepping
run_test "Debugger Interactive Mode" \
    "submit $TEST_DIR/valid_simple.lang\ndebug 1\nstep\nquit" \
    "Debugger started" \
    "Segmentation fault"

# ==============================================================================
#  SUMMARY
# ==============================================================================
echo -e "${BLUE}======================================================${NC}"
echo -e "      Full logs available in: $LOG_FILE"
echo -e "${BLUE}======================================================${NC}"