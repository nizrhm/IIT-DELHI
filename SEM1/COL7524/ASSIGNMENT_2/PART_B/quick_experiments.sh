#!/bin/bash

echo "🚀 Quick Load Balancer Experiment Runner"
echo "=========================================="

# Kill any existing processes
echo "🧹 Cleaning up..."
pkill -f backend_server 2>/dev/null
pkill -f load_balancer 2>/dev/null
sleep 2

# Clean files
rm -f *.log *.csv *.txt downloaded_* testfile*
rm -rf server_storage_* plots

# Build
echo "🔨 Building system..."
make clean
make

# Test parameters
ALGORITHMS=("round_robin" "least_connections")
WORKLOADS=("mixed" "write_heavy" "read_heavy")

for algo in "${ALGORITHMS[@]}"; do
    for workload in "${WORKLOADS[@]}"; do
        echo ""
        echo "🔬 Testing $algo with $workload workload..."
        
        # Start servers
        echo "🚀 Starting servers..."
        for port in 9001 9002 9003 9004; do
            ./backend_server 127.0.0.1 $port &
        done
        sleep 3
        
        # Start load balancer
        echo "🔀 Starting load balancer ($algo)..."
        ./load_balancer --algorithm $algo &
        LB_PID=$!
        sleep 3
        
        # Generate test files
        echo "📁 Generating test files..."
        for i in {1..10}; do
            echo "Content of test file $i for $workload" > "testfile_${i}.txt"
        done
        
        # Run workload
        echo "💻 Running $workload workload..."
        
        if [ "$workload" == "mixed" ]; then
            # PUT all files, then GET some
            for i in {1..10}; do
                ./client PUT "testfile_${i}.txt" &
            done
            wait
            for i in {1..5}; do
                ./client GET "testfile_${i}.txt" &
            done
        elif [ "$workload" == "write_heavy" ]; then
            # Only PUT operations
            for i in {1..15}; do
                ./client PUT "testfile_${i}.txt" &
            done
        elif [ "$workload" == "read_heavy" ]; then
            # PUT then GET all
            for i in {1..8}; do
                ./client PUT "testfile_${i}.txt"
            done
            for i in {1..8}; do
                ./client GET "testfile_${i}.txt" &
            done
        fi
        
        wait
        sleep 2
        
        # Cleanup for next test
        kill $LB_PID 2>/dev/null
        pkill -f backend_server 2>/dev/null
        sleep 2
        rm -f testfile_*.txt downloaded_*.txt
        rm -rf server_storage_*
    done
done

echo ""
echo "📈 Running analysis..."
python3 analyze_logs.py

echo ""
echo "✅ Experiment completed!"
echo "📊 Check the 'plots' directory for analysis results"