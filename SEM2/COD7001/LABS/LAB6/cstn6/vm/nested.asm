; --- Main Program ---
PUSH 10
CALL outer      ; Jump to outer function
ADD             ; Should result in 10 + 25 = 35
HALT

; --- Outer Function ---
outer:
    PUSH 5
    CALL inner  ; Nested call to inner function
    RET         ; Return to Main

; --- Inner Function (Square) ---
inner:
    DUP
    MUL
    RET         ; Return to Outer