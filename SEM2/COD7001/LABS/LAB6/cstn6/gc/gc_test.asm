; Lab 5 GC Test Script
PUSH 10
PUSH 20
ADD      ; Result 30 (Object 1 survives)
PUSH 40  ; Object 2 created
POP      ; Object 2 becomes unreachable
GC       ; Reclaims Object 2, preserves 30
HALT