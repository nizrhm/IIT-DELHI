#!/bin/bash

echo "=== Load Balancer Test with Custom Log Formats ==="

# Kill existing processes
pkill -f backend_server
pkill -f load_balancer
sleep 2

# Clean up
rm -f *.log *.csv downloaded_* testfile* 
rm -rf server_storage_*

# Build
make clean
make

# Start all 4 backend servers
echo "Starting all 4 backend servers..."
for port in 9001 9002 9003 9004; do
    ./backend_server 127.0.0.1 $port &
    sleep 0.5
done

echo "Waiting for servers to start..."
sleep 3

# Start load balancer
echo "Starting load balancer..."
./load_balancer &
LB_PID=$!

echo "Waiting for load balancer to initialize..."
sleep 5

# Test operations
echo "Testing PUT/GET operations..."

# Create test files
echo "Creating test files..."
for i in {1..3}; do
    echo "This is test file $i" > "testfile$i.txt"
done

# PUT files
echo "PUT operations:"
for i in {1..3}; do
    echo "Uploading testfile$i.txt..."
    ./client put "testfile$i.txt"
    sleep 1
done

# GET files  
echo "GET operations:"
for i in {1..3}; do
    echo "Downloading testfile$i.txt..."
    ./client get "testfile$i.txt"
    sleep 1
done

# Show results
echo ""
echo "=== Test Results ==="
echo "Generated log files:"
ls -la *.csv 2>/dev/null || echo "No CSV log files found"

# Show log contents
for csvfile in health_log_*.csv forward_log_*.csv; do
    if [ -f "$csvfile" ]; then
        echo ""
        echo "=== $csvfile ==="
        head -10 "$csvfile"
        echo "..."
        wc -l "$csvfile"
    fi
done

# Cleanup
echo ""
echo "Cleaning up..."
kill $LB_PID 2>/dev/null
pkill -f backend_server 2>/dev/null

echo "Test completed!"