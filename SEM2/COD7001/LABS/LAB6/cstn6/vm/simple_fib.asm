PUSH 0    ; a = 0
PUSH 1    ; b = 1
DUP       ; Copy b
ROT_ADD:  ; Manually doing a + b
ADD       ; Result: 1 (0+1)
DUP       ; Copy result
PUSH 1    ; Load previous b
ADD       ; Result: 2 (1+1)
HALT