# GDB Script for SMP demo (2, 4, or 8 harts)
# Load: riscv-none-elf-gdb -batch -ex "set $expected_harts=4" -x gdb_demo_smp.gdb examples/test.s

set confirm off
set pagination off
set mem inaccessible-by-default off

set $last_thread = $expected_harts
set $worker_pc = 0x80000034

printf "\n=== RISC-V Emulator SMP Demo (%d harts) ===\n", $expected_harts

target remote localhost:1234

echo \n--- Thread List ---\n
info threads

echo \n--- Hart 0 (runs demo program) ---\n
thread 1
info registers tp pc t1 t2 s0
x/8i $pc

echo \n--- Worker harts poll at 0x80000034 (RV32I handler at 0x80000054) ---\n
thread 2
info registers tp pc s1 s2
thread $last_thread
info registers tp pc

echo \n--- Shared sync region at 0x80002000 ---\n
x/4w 0x80002000

echo \n--- Breakpoint on hart 0 loop ---\n
thread 1
break *0x80000014
continue
info registers t1 t2 s0

echo \n--- Dispatch work via loop store (IPI + spinlock) ---\n
stepi
echo After one store:\n
x/4w 0x80002000
thread 2
info registers s1 s2

echo \n--- Per-hart continue: step only hart 1 (thread 2) ---\n
thread 2
maint packet Hcp1.2
stepi
echo Hart 1 stepped in worker loop; hart 0 PC unchanged:\n
thread 1
info registers pc
maint packet Hcp1.-1

echo \n--- Watchpoint on hart 0 ---\n
thread 1
delete breakpoints
watch *(int*)0x80001000
stepi
info registers t1 t3
x/2w 0x80001000

echo \n--- SMP Demo Complete ---\n
quit
