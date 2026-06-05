#!/bin/bash
# sudo ./run_dynamic_shaping.sh <iface> <duration_sec>

IFACE=$1
DURATION=$2

BASE_DIR=$(cd "$(dirname "$0")" && pwd)
LOG_FILE="shaping_log.csv"

echo "timestamp,down_mbps,up_mbps,delay_ms" > "$LOG_FILE"

START=$(date +%s)

while true; do
    NOW=$(date +%s)
    [ $((NOW - START)) -ge $DURATION ] && break

    DOWN=$(awk -v min=0.1 -v max=4 'BEGIN{srand(); print min+rand()*(max-min)}')
    UP=$DOWN
    DELAY=$((RANDOM % 181 + 20))
    TS=$(date +%s)

    sudo "$BASE_DIR/shaper.sh" update "$IFACE" "$DOWN" "$UP" "$DELAY"
    echo "$TS,$DOWN,$UP,$DELAY" >> "$LOG_FILE"

    sleep 1
done
