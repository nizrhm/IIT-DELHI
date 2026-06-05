PUSH 50
STORE 0         ; Memory[0] = 50
PUSH 5          ; Loop counter (n=5)

loop:
    DUP
    JZ end      ; If counter is 0, jump to end
    
    ; Increment Memory[0]
    LOAD 0
    PUSH 1
    ADD
    STORE 0
    
    ; Decrement counter
    PUSH 1
    SUB
    JMP loop

end:
    POP         ; Remove 0 from stack
    LOAD 0      ; Load final value (Should be 55)
    HALT