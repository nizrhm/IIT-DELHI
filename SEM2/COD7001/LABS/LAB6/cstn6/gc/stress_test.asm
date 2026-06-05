; Stress test: Allocate many objects then trigger GC
PUSH 10
loop:
    DUP
    JZ end
    PUSH 1
    SUB
    GC      ; Trigger GC frequently [cite: 23]
    JMP loop
end:
    HALT