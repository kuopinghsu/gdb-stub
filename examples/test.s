.section .text
.globl _start
_start:
    lui  t0, 0x80000      # Load base address
    addi t1, zero, 0      # Counter = 0
    addi t2, zero, 5      # Limit = 5 (reduced for demo)
    lui  s0, 0x80001      # Memory area for watchpoint demo
    addi s0, s0, 0        # s0 = 0x80001000 (watchpoint target)
    
loop:
    # Store counter value to memory (triggers write watchpoint)
    sw   t1, 0(s0)        # Store counter to 0x80001000
    
    # Load value back from memory (triggers read watchpoint)  
    lw   t3, 0(s0)        # Load from 0x80001000 into t3
    
    # Increment counter
    addi t1, t1, 1        # counter++
    
    # Check if we've reached the limit
    beq  t1, t2, end      # if counter == limit, go to end
    jal  loop             # jump back to loop
    
end:
    # Final store operation
    sw   t1, 4(s0)        # Store final value to 0x80001004
    jal  end              # Infinite loop at end (no ecall)
