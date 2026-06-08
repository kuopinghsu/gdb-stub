# GDB Script for SMP demo (2, 4, or 8 harts)
# Load: riscv-none-elf-gdb -batch -ex "set $expected_harts=4" -x gdb_demo_smp.gdb examples/test.s

set confirm off
set pagination off
set mem inaccessible-by-default off

if !$_exists($expected_harts)
  set $expected_harts = 4
end

set $last_thread = $expected_harts

printf "\n=== RISC-V Emulator SMP Demo (%d harts) ===\n", $expected_harts

target remote localhost:1234

echo \n--- Thread List ---\n
info threads

echo \n--- Hart 0 (runs demo program) ---\n
thread 1
info registers tp pc t1 t2 s0
x/8i $pc

echo \n--- Secondary harts idle at 0x8000002c ---\n
thread 2
info registers tp pc
thread $last_thread
info registers tp pc

echo \n--- Breakpoint on hart 0 loop ---\n
thread 1
break *0x80000014
continue
info registers t1 t2 s0

echo \n--- Per-hart continue: step only hart 1 (thread 2) ---\n
thread 2
maint packet Hc2
stepi
echo Hart 1 stepped in idle loop; hart 0 PC unchanged:\n
thread 1
info registers pc
maint packet Hc-1

echo \n--- Watchpoint on hart 0 ---\n
thread 1
delete breakpoints
watch *(int*)0x80001000
stepi
info registers t1 t3
x/2w 0x80001000

echo \n--- SMP Demo Complete ---\n
quit
