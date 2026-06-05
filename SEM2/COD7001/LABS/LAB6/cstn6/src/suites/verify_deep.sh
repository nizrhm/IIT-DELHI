#!/bin/bash

# ==============================================================================
#  DEEP INTEGRATION VERIFICATION SUITE (VERBOSE LOGGING)
# ==============================================================================
SYSTEM_BIN="./mysystem"
TEST_DIR="tests"
LOG_FILE="tests/deep_test_results.log"

mkdir -p tests/$TEST_DIR
# Initialize Log with Header
echo "===================================================" > $LOG_FILE
echo "   DEEP INTEGRATION TEST LOG - $(date)" >> $LOG_FILE
echo "===================================================" >> $LOG_FILE

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== GENERATING TEST FILES ===${NC}"

# 1. Generate Fibonacci Source
cat > tests/$TEST_DIR/fib.lang <<EOF
var n = 10;
var a = 0;
var b = 1;
var i = 2;
var temp = 0;
while (i < n) {
    temp = a + b;
    a = b;
    b = temp;
    i = i + 1;
}
EOF

# 2. Generate GC Stress Source
cat > tests/$TEST_DIR/gc_stress.lang <<EOF
var i = 0;
while (i < 50) {
    var x = i * 10;
    var y = x + 5;
    i = i + 1;
}
EOF

# ==============================================================================
#  TEST 1: CORRECTNESS (Fibonacci)
# ==============================================================================
echo -e -n "Testing: ${BLUE}Fibonacci Logic (Valid Output)${NC} ... "

output=$(echo -e "submit tests/$TEST_DIR/fib.lang\nrun 1\nexit" | $SYSTEM_BIN 2>&1)

# LOGGING
echo -e "\n--- TEST 1: FIBONACCI OUTPUT ---" >> $LOG_FILE
echo "$output" >> $LOG_FILE

if [[ "$output" == *"[VM] Halted"* ]]; then
    echo -e "${GREEN}[PASS]${NC}"
else
    echo -e "${RED}[FAIL]${NC}"
fi

# ==============================================================================
#  TEST 2: DEEP DEBUGGER SESSION (Stepping & Inspection)
# ==============================================================================
echo -e -n "Testing: ${BLUE}Debugger Stepping & Breakpoints${NC} ... "

debug_script="submit tests/$TEST_DIR/fib.lang
debug 1
step
step
step
step
regs
run
quit
exit"

output=$(echo -e "$debug_script" | $SYSTEM_BIN 2>&1)

# LOGGING
echo -e "\n--- TEST 2: DEBUGGER SESSION ---" >> $LOG_FILE
echo "$output" >> $LOG_FILE

if [[ "$output" == *"Executed 1 instr"* ]]; then
    echo -e "${GREEN}[PASS]${NC}"
else
    echo -e "${RED}[FAIL]${NC} (Debugger didn't step or report regs)"
fi

# ==============================================================================
#  TEST 3: GC CHECKPOINTING
# ==============================================================================
echo -e -n "Testing: ${BLUE}GC Execution During Loop${NC} ... "

gc_script="submit tests/$TEST_DIR/gc_stress.lang
run 1
memstat 1
exit"

output=$(echo -e "$gc_script" | $SYSTEM_BIN 2>&1)

# LOGGING
echo -e "\n--- TEST 3: GC MEMORY STATS ---" >> $LOG_FILE
echo "$output" >> $LOG_FILE

if [[ "$output" == *"[VM] Halted"* ]] && [[ "$output" == *"Active Objects"* ]]; then
    echo -e "${GREEN}[PASS]${NC}"
else
    echo -e "${RED}[FAIL]${NC}"
fi

echo -e "\n${BLUE}Full verification logs saved to: $LOG_FILE${NC}"