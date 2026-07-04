# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Isrc

# Source Files
SRC = src/main.cpp \
      src/runtime.cpp \
      src/scheduler.cpp \
      src/monitor.cpp \
      src/policy_engine.cpp \
      src/Metrics.cpp \
      src/deadlock_detector.cpp \
      src/watchdog.cpp \
      workloads/cpu_workload.cpp \
      workloads/memory_workload.cpp \
      workloads/mixed.cpp \
      workloads/phase_change.cpp

# Output Binary
OUT = runtime

# Phony Targets
.PHONY: all clean clean-data test

# Default Target: Build the runtime
all:
	@echo "Building Resource Manager..."
	$(CXX) $(CXXFLAGS) $(SRC) -o $(OUT)
	@echo "Build complete: ./$(OUT)"

# Clean Target: Remove binaries and logs
clean:
	rm -f $(OUT) unit_test unit_test_output.txt evaluation_results.csv adaptation_log.txt test_workload.csv
	@echo "Cleaned binary, test artifacts, and log files."

# Clean Data Target: Remove only logs and results
clean-data:
	rm -f evaluation_results.csv adaptation_log.txt
	@echo "Removed logs and results."

# Test Target: Run unit tests and verification suite
test: all
	@echo "Building unit tests..."
	$(CXX) $(CXXFLAGS) -Isrc src/unit_tests.cpp src/deadlock_detector.cpp src/watchdog.cpp src/test_stubs.cpp -o unit_test
	@echo "Running unit tests..."
	./unit_test
	@echo "Running full verification suite..."
	bash run_tests.sh