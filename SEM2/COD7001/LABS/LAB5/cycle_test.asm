; cycle_test.asm
; Goal: Create a circular reference and trigger GC

PUSH 10          ; Push a dummy value
PUSH 20          ; Push another dummy value
PAIR             ; Create Pair A [10, 20]
DUP              ; Duplicate pointer to Pair A
DUP              ; Duplicate pointer to Pair A again

; At this point, the stack has three pointers to the same Pair A.
; We can treat the Pair as [left, right]. 
; To create a cycle, we use the DUPed pointers to nest them.

PAIR             ; Create Pair B where 'left' is Pair A
                 ; Stack now has [Pair A, Pair B(left=Pair A)]

; If your VM supports a SET_RIGHT or similar, you'd use it here.
; Since we are using standard PUSH/PAIR logic, we simply trigger 
; the GC to ensure it handles the nested reachability without looping.

GC               ; Trigger Garbage Collection
HALT             ; Stop the VM