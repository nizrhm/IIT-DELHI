; --- Optimized app.asm ---
JMP main          ; Skip over library/subroutines to start at main logic

INCLUDE math_lib.asm

main:
    ; --- Part 1: Test Max ---
    PUSH 50          
    PUSH 20          
    CALL max         ; Stack now has [50]
    
    ; --- Part 2: Test Zetter ---
    CALL zetter      ; Stack now has [50, 35]
    HALT             ; Termination [cite: 118]

zetter:
    PUSH 10
    CALL outer      
    ADD             
    RET              ; REQUIRED: Return to main [cite: 128]

outer:
    PUSH 5
    CALL square     
    RET              ; Return to zetter [cite: 128]