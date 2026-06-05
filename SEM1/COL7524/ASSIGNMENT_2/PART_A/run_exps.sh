#!/bin/bash

# ======== EXPERIMENT SETTINGS ========
SERVER_THREADS=4
CLIENT_THREADS=8
PACKET_LINES=10
OPS_PER_THREAD=10
BASE_DIR="./server_files"
BENCH_DIR="./client_files"
QUANTUM=5  # for RR scheduler

SCHEDS=("fcfs" "sjf" "rr")

mkdir -p logs

for sched in "${SCHEDS[@]}"; do
    echo "=== Running $sched | Server threads=$SERVER_THREADS | Client threads=$CLIENT_THREADS | p=$PACKET_LINES ==="

    LOGFILE="logs/${sched}_log.csv"
    > $LOGFILE  # clear previous logs

    # start server
    if [ "$sched" = "rr" ]; then
        ./server --sched rr --quantum $QUANTUM --file $BASE_DIR --p $PACKET_LINES &
    else
        ./server --sched $sched --file $BASE_DIR --p $PACKET_LINES &
    fi

    SERVER_PID=$!
    echo "Server PID: $SERVER_PID"

    # wait until server is listening
    sleep 1  # adjust if necessary

    # start benchmark client
    ./client BENCH $BENCH_DIR $OPS_PER_THREAD

    # wait a bit for all requests to finish
    sleep 2

    # terminate server
    kill -INT $SERVER_PID
    wait $SERVER_PID 2>/dev/null

    # copy server log for this scheduler
    cp server_log.csv $LOGFILE

    echo "Logs saved to $LOGFILE"
done

echo "=== All experiments completed ==="
