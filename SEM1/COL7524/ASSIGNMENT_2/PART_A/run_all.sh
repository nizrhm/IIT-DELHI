#!/bin/bash

# ---------------- Config ----------------
SERVER="./server"
CLIENT="./client"
CONFIG="config.json"
SERVER_DIR="./server_files"
CLIENT_DIR="./client_files"
DOWNLOAD_DIR="./bench_downloads"
LOG_SCRIPT="process_logs.py"
LOG_SCRIPT_ALL="process_logs_all.py"

mkdir -p "$SERVER_DIR" "$CLIENT_DIR" "$DOWNLOAD_DIR"

# ---------------- Menu ----------------
while true; do
    echo "======================"
    echo "  Assignment 2 Menu"
    echo "======================"
    echo "1) Start Server"
    echo "2) Client PUT"
    echo "3) Client GET"
    echo "4) Run Benchmark"
    echo "5) Process Logs (single)"
    echo "6) Process Logs (all)"
    echo "7) Exit"
    echo -n "Choose an option: "
    read opt

    case $opt in
        1)
            echo -n "Enter scheduling policy (fcfs/sjf/rr): "
            read sched
            quantum=""
            if [ "$sched" == "rr" ]; then
                echo -n "Enter quantum (ms): "
                read quantum
            fi
            echo -n "Enter packetization (--p): "
            read p
            echo "Starting server..."
            if [ "$sched" == "rr" ]; then
                $SERVER --sched $sched --quantum $quantum --file $SERVER_DIR --p $p &
            else
                $SERVER --sched $sched --file $SERVER_DIR --p $p &
            fi
            SERVER_PID=$!
            echo "Server PID: $SERVER_PID"
            ;;
        2)
            echo -n "Enter local file path to upload: "
            read local
            echo -n "Enter remote file name on server: "
            read remote
            $CLIENT PUT "$local" "$remote"
            ;;
        3)
            echo -n "Enter remote file name on server: "
            read remote
            echo -n "Enter local path or directory to save: "
            read local
            $CLIENT GET "$remote" "$local"
            ;;
        4)
            echo -n "Enter workdir for benchmark: "
            read workdir
            echo -n "Enter ops per thread: "
            read ops
            $CLIENT BENCH "$workdir" "$ops"
            ;;
        5)
            echo "Processing server_log.csv..."
            python3 "$LOG_SCRIPT"
            ;;
        6)
            echo "Processing all logs and generating plots..."
            python3 "$LOG_SCRIPT_ALL"
            ;;
        7)
            echo "Shutting down..."
            if [ ! -z "$SERVER_PID" ]; then
                kill $SERVER_PID 2>/dev/null
            fi
            exit 0
            ;;
        *)
            echo "Invalid option. Try again."
            ;;
    esac
done
