; -----------------------------------------
; Fibonacci using memory + loop
; Computes F(n) where n = 7
; Result will be on top of stack
; -----------------------------------------

PUSH 7
STORE 0          ; Memory[0] = n

PUSH 0
STORE 1          ; Memory[1] = a = 0

PUSH 1
STORE 2          ; Memory[2] = b = 1

; -------- LOOP START --------
loop:
    LOAD 0       ; push n
    PUSH 1
    CMP          ; push 1 if n < 1, else 0
    JNZ done     ; if n < 1 -> jump to done (answer is a)

    ; temp = a + b
    LOAD 1       ; push a
    LOAD 2       ; push b
    ADD
    STORE 3      ; Memory[3] = temp

    ; a = b
    LOAD 2
    STORE 1

    ; b = temp
    LOAD 3
    STORE 2

    ; n = n - 1
    LOAD 0
    PUSH 1
    SUB
    STORE 0

    JMP loop     ; repeat
; -------- LOOP END --------

done:
    LOAD 1       ; push result (a)
    HALT
