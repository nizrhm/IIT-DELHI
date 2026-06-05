#!/bin/bash
set -e

IFACE=$1
NUM_EXP=50
DURATION=120

BASE_DIR=$(cd "$(dirname "$0")" && pwd)
PY="$BASE_DIR/venv/bin/python"

if [ ! -x "$PY" ]; then
    echo "ERROR: Python not found at $PY"
    exit 1
fi

mkdir -p "$BASE_DIR/chatgpt"
cd "$BASE_DIR/chatgpt"


for i in $(seq 1 $NUM_EXP); do
    TS=$(date +%Y%m%d_%H%M%S)
    TMP_DIR="experiment_$TS"

    echo "[*] Experiment $i / $NUM_EXP"
    mkdir "$TMP_DIR"
    cd "$TMP_DIR"

    sudo "$BASE_DIR/shaper.sh" start "$IFACE" 1 1 50
    sudo "$BASE_DIR/run_dynamic_shaping.sh" "$IFACE" "$DURATION" &
    SHAPER_PID=$!

    sudo tcpdump -i "$IFACE" -w capture.pcap &
    TCPDUMP_PID=$!
    sleep 3

    "$PY" "$BASE_DIR/part2_chatgpt.py"

    kill "$SHAPER_PID"
    sudo "$BASE_DIR/shaper.sh" stop "$IFACE"
    sudo kill "$TCPDUMP_PID"

    STATS=$("$PY" - <<EOF
from network_stats import compute_stats
bw, bw_std, lat, lat_std = compute_stats("shaping_log.csv")
print(f"{bw}kbps_{bw_std}kbps_{lat}ms_{lat_std}ms")
EOF
)

    FINAL_DIR="experiment_${TS}_${STATS}"
    cd ..
    mv "$TMP_DIR" "$FINAL_DIR"

    sleep 10
done

echo "[✓] All 50 ChatGPT experiments completed"
