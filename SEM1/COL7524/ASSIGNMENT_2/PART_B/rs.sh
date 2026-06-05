#!/bin/bash

echo "Load Balancer Experiment"
echo "========================"

# Build all components
echo "Building components..."
make clean
make

if [ $? -ne 0 ]; then
    echo "Build failed! Please check the errors above."
    exit 1
fi

echo "Build successful!"

# Start backend servers in background
echo "Starting backend servers..."
chmod +x start_servers.sh
./start_servers.sh &
SERVERS_PID=$!

# Wait for servers to start
echo "Waiting for servers to initialize..."
sleep 5

# Test both algorithms
for algorithm in "weighted" "least_connections"; do
    echo ""
    echo "=========================================="
    echo "Testing Algorithm: $algorithm"
    echo "=========================================="
    
    # Start load balancer in background
    echo "Starting Load Balancer with $algorithm algorithm..."
    ./load_balancer --algorithm $algorithm &
    LB_PID=$!
    
    # Wait for LB to start
    echo "Waiting for load balancer to start..."
    sleep 3
    
    # Check if LB is running
    if ! ps -p $LB_PID > /dev/null; then
        echo "ERROR: Load balancer failed to start!"
        continue
    fi
    
    # Create test files
    echo "Creating test files..."
    for i in {1..3}; do
        echo "This is test file $i for algorithm $algorithm" > "testfile${i}_${algorithm}.txt"
    done
    
    # Perform PUT operations
    echo "Performing PUT operations..."
    for i in {1..3}; do
        echo "Uploading testfile${i}_${algorithm}.txt..."
        ./client PUT "testfile${i}_${algorithm}.txt"
        sleep 0.5
    done
    
    # Perform GET operations  
    echo "Performing GET operations..."
    for i in {1..3}; do
        echo "Downloading testfile${i}_${algorithm}.txt..."
        ./client GET "testfile${i}_${algorithm}.txt"
        sleep 0.5
    done
    
    # Stop load balancer
    echo "Stopping Load Balancer..."
    kill $LB_PID 2>/dev/null
    wait $LB_PID 2>/dev/null
    
    echo "Algorithm $algorithm test completed."
    sleep 2
done

# Cleanup
echo ""
echo "Cleaning up..."
kill $SERVERS_PID 2>/dev/null
sleep 2

echo ""
echo "Experiment completed!"
echo "Check the generated log files for analysis:"
ls -la *.log 2>/dev/null || echo "No log files found"