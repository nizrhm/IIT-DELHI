#!/bin/bash

echo "=========================================="
echo "Starting Backend Servers for Load Balancer"
echo "=========================================="

# Kill any existing servers on these ports
echo "Cleaning up existing processes..."
pkill -f "backend_server 127.0.0.1 9001" || true
pkill -f "backend_server 127.0.0.1 9002" || true
pkill -f "backend_server 127.0.0.1 9003" || true
pkill -f "backend_server 127.0.0.1 9004" || true

sleep 2

# Start backend servers on ports 9001-9004
echo "Starting Server 1 on port 9001..."
./backend_server 127.0.0.1 9001 &
SERVER1_PID=$!
sleep 1

echo "Starting Server 2 on port 9002..."
./backend_server 127.0.0.1 9002 &
SERVER2_PID=$!
sleep 1

echo "Starting Server 3 on port 9003..."
./backend_server 127.0.0.1 9003 &
SERVER3_PID=$!
sleep 1

echo "Starting Server 4 on port 9004..."
./backend_server 127.0.0.1 9004 &
SERVER4_PID=$!
sleep 1

echo "=========================================="
echo "All backend servers started successfully!"
echo "Server PIDs: $SERVER1_PID, $SERVER2_PID, $SERVER3_PID, $SERVER4_PID"
echo "=========================================="
echo "Press Ctrl+C to stop all servers"

# Function to cleanup on exit
cleanup() {
    echo ""
    echo "Stopping all backend servers..."
    kill $SERVER1_PID $SERVER2_PID $SERVER3_PID $SERVER4_PID 2>/dev/null
    echo "All servers stopped."
    exit 0
}

# Set trap for cleanup
trap cleanup SIGINT

# Wait indefinitely
while true; do
    sleep 1
done