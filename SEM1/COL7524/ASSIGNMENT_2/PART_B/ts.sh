#!/bin/bash

echo "=== Load Balancer Complete Test ==="

# Kill existing processes
pkill -f backend_server
pkill -f load_balancer
sleep 2

# Clean up
rm -f *.log downloaded_* testfile* 
rm -rf server_storage_*

# Build
make clean
make

# Test both algorithms
for algo in "round_robin" "least_connections"; do
    echo ""
    echo "=== Testing Algorithm: $algo ==="
    
    # Start all 4 backend servers
    echo "Starting all 4 backend servers..."
    for port in 9001 9002 9003 9004; do
        ./backend_server 127.0.0.1 $port &
        sleep 0.5
    done

    echo "Waiting for servers to start..."
    sleep 3

    # Start load balancer with specific algorithm
    echo "Starting load balancer with $algo algorithm..."
    ./load_balancer --algorithm $algo &
    LB_PID=$!

    echo "Waiting for load balancer to initialize..."
    sleep 3

    # Test operations
    echo "Testing PUT/GET operations..."

    # Create test files
    echo "Creating test files..."
    for i in {1..3}; do
        echo "This is test file $i for $algo" > "testfile${i}_${algo}.txt"
    done

    # PUT files
    echo "PUT operations:"
    for i in {1..3}; do
        echo "Uploading testfile${i}_${algo}.txt..."
        ./client put "testfile${i}_${algo}.txt"
        sleep 0.5
    done

    # GET files  
    echo "GET operations:"
    for i in {1..3}; do
        echo "Downloading testfile${i}_${algo}.txt..."
        ./client get "testfile${i}_${algo}.txt"
        sleep 0.5
    done

    # Show results
    echo ""
    echo "=== Results for $algo ==="
    if ls downloaded_* >/dev/null 2>&1; then
        echo "Downloaded files:"
        ls -la downloaded_*
        echo "File contents:"
        for file in downloaded_*; do
            echo "=== $file ==="
            cat "$file"
        done
    else
        echo "No files were downloaded successfully"
    fi

    # Cleanup for next algorithm test
    kill $LB_PID 2>/dev/null
    pkill -f backend_server 2>/dev/null
    sleep 2
    rm -f downloaded_* testfile*_${algo}.txt
    rm -rf server_storage_*
done

echo ""
echo "=== All Tests Completed ==="