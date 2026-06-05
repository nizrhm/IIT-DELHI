#!/bin/bash

# Usage:
# sudo ./tc_shape.sh start  <iface> <down_mbps> <up_mbps> <delay_ms>
# sudo ./tc_shape.sh update <iface> <down_mbps> <up_mbps> <delay_ms>
# sudo ./tc_shape.sh stop   <iface>

ACTION=$1
IFACE=$2
DOWN_MBIT=$3
UP_MBIT=$4
DELAY_MS=$5

IFB=ifb0

start() {
    echo "[*] Starting traffic shaping on $IFACE"

    modprobe ifb
    ip link add $IFB type ifb 2>/dev/null
    ip link set dev $IFB up

    # Cleanup
    tc qdisc del dev $IFACE root 2>/dev/null
    tc qdisc del dev $IFACE ingress 2>/dev/null
    tc qdisc del dev $IFB root 2>/dev/null

    # ----------------------
    # Upload (egress)
    # ----------------------
    tc qdisc add dev $IFACE root handle 1: htb default 10
    tc class add dev $IFACE parent 1: classid 1:10 htb rate ${UP_MBIT}mbit

    tc qdisc add dev $IFACE parent 1:10 handle 10: netem delay ${DELAY_MS}ms

    # ----------------------
    # Download (ingress via IFB)
    # ----------------------
    tc qdisc add dev $IFACE handle ffff: ingress

    tc filter add dev $IFACE parent ffff: protocol ip u32 \
        match u32 0 0 action mirred egress redirect dev $IFB

    tc qdisc add dev $IFB root handle 1: htb default 10
    tc class add dev $IFB parent 1: classid 1:10 htb rate ${DOWN_MBIT}mbit

    tc qdisc add dev $IFB parent 1:10 handle 10: netem delay ${DELAY_MS}ms

    echo "[✓] Shaping started:"
    echo "    Down:  ${DOWN_MBIT} Mbps"
    echo "    Up:    ${UP_MBIT} Mbps"
    echo "    Delay: ${DELAY_MS} ms"
}

update() {
    echo "[*] Updating shaping parameters on $IFACE"

    # Update upload rate
    tc class change dev $IFACE parent 1: classid 1:10 htb rate ${UP_MBIT}mbit

    # Update upload delay
    tc qdisc change dev $IFACE parent 1:10 netem delay ${DELAY_MS}ms

    # Update download rate
    tc class change dev $IFB parent 1: classid 1:10 htb rate ${DOWN_MBIT}mbit

    # Update download delay
    tc qdisc change dev $IFB parent 1:10 netem delay ${DELAY_MS}ms

    echo "[✓] Shaping updated:"
    echo "    Down:  ${DOWN_MBIT} Mbps"
    echo "    Up:    ${UP_MBIT} Mbps"
    echo "    Delay: ${DELAY_MS} ms"
}

stop() {
    echo "[*] Stopping traffic shaping on $IFACE"

    tc qdisc del dev $IFACE root 2>/dev/null
    tc qdisc del dev $IFACE ingress 2>/dev/null
    tc qdisc del dev $IFB root 2>/dev/null

    ip link set dev $IFB down 2>/dev/null
    ip link del $IFB 2>/dev/null

    echo "[✓] Shaping disabled"
}

case "$ACTION" in
    start)
        start
        ;;
    update)
        update
        ;;
    stop)
        stop
        ;;
    *)
        echo "Usage:"
        echo "  sudo $0 start  <iface> <down_mbps> <up_mbps> <delay_ms>"
        echo "  sudo $0 update <iface> <down_mbps> <up_mbps> <delay_ms>"
        echo "  sudo $0 stop   <iface>"
        exit 1
        ;;
esac

