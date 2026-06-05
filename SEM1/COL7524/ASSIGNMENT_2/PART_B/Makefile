CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra -O2
TARGET_LB = load_balancer
TARGET_SERVER = backend_server
TARGET_CLIENT = client

all: $(TARGET_LB) $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_LB): load_balancer.cpp config_parser.h
	$(CXX) $(CXXFLAGS) -o $(TARGET_LB) load_balancer.cpp

$(TARGET_SERVER): backend_server.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET_SERVER) backend_server.cpp

$(TARGET_CLIENT): client.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET_CLIENT) client.cpp

# Experiment targets
experiment: all
	@echo "🚀 Running comprehensive experiments..."
	@python3 run_experiments.py

quick-test: all
	@echo "⚡ Running quick experiments..."
	@chmod +x quick_experiment.sh
	@./quick_experiment.sh

analysis:
	@echo "📊 Running analysis on existing logs..."
	@python3 alogs.py

clean-all: clean
	@rm -f *.csv *.log experiment_*.txt experiment_summary.txt experiment_results.csv
	@rm -rf plots
	@pkill -f backend_server 2>/dev/null || true
	@pkill -f load_balancer 2>/dev/null || true

clean:
	rm -f $(TARGET_LB) $(TARGET_SERVER) $(TARGET_CLIENT) *.log downloaded_* testfile* 
	rm -rf server_storage_*

.PHONY: all clean clean-all experiment quick-test analysis