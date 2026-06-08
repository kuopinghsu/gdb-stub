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
    jal  end              # Infinite loop at end (0x8000002c)

# Worker hart RV32I code (matches emu/smp_program.h at 0x80000030)
worker_wait:
    lui  s0, 0x80002      # s0 = 0x80002000 sync base
worker_poll:
    addi t0, s0, 0x10     # &ipi_pending[0]
    slli t3, tp, 2
    add  t0, t0, t3
    lw   t1, 0(t0)        # ipi_pending[tp]
    beq  t1, zero, worker_spin
    sw   zero, 0(t0)      # clear IPI
    jal  ra, worker_handle
worker_spin:
    jal  zero, worker_poll
worker_handle:
    lw   t0, 0(s0)        # acquire lock
    bne  t0, zero, worker_handle
    addi t0, tp, 1
    sw   t0, 0(s0)
    lw   t2, 8(s0)        # work_ready
    beq  t2, zero, worker_release
    lw   t1, 4(s0)        # work_item
    sw   zero, 8(s0)
    addi s1, t1, 0
    lw   t2, 12(s0)       # shared_done
    addi t2, t2, 1
    sw   t2, 12(s0)
    addi s2, t2, 0
worker_release:
    sw   zero, 0(s0)      # release lock
    jal  zero, worker_poll
