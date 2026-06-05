; --- Standard Math Library ---

; SQUARE: Pops x, pushes x*x
square:
    DUP
    MUL
    RET

; MAX: Pops b, pops a, pushes the larger of the two
max:
    STORE 100    ; Store b in temp
    STORE 101    ; Store a in temp
    LOAD 101
    LOAD 100
    CMP          ; Push 1 if a < b
    JNZ b_is_max ; If 1 (a < b), jump to b
    LOAD 101     ; Else, a is max
    RET
b_is_max:
    LOAD 100
    RET

; MIN: Pops b, pops a, pushes the smaller of the two
min:
    STORE 100
    STORE 101
    LOAD 101
    LOAD 100
    CMP          ; Push 1 if a < b
    JNZ a_is_min
    LOAD 100     ; b is smaller
    RET
a_is_min:
    LOAD 101
    RET