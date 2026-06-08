# GDB Script for RISC-V Emulator Demo (single hart, -c 1)
# Load this script with: riscv-none-elf-gdb -batch -x gdb_demo.gdb examples/test.s

set confirm off
set pagination off
set mem inaccessible-by-default off

echo \n=== RISC-V Emulator GDB Demo ===\n
echo This demo will showcase watchpoint functionality\n

# Connect to the emulator
target remote localhost:1234

# Show initial state
echo \n--- Initial CPU State ---\n
info registers

# Show the program code
echo \n--- Program Disassembly ---\n
x/15i $pc

# Set a breakpoint at the main loop
echo \n--- Setting Breakpoint at Loop ---\n
break *0x80000014

# Show current breakpoints
info breakpoints

# Continue execution to first breakpoint
echo \n--- Continuing to Loop Breakpoint ---\n
continue

# Show registers at breakpoint
echo \n--- Registers at Loop Start ---\n
info registers t1 t2 s0

# Set up watchpoints (GDB resumes automatically after each watch command)
echo \n--- Setting Up Watchpoints ---\n
echo Setting write watchpoint on 0x80001000...\n
delete breakpoints
watch *(int*)0x80001000

echo \n--- After Write Watchpoint Hit ---\n
info registers t1 t3
x/2w 0x80001000

echo Setting read watchpoint on 0x80001000...\n
rwatch *(int*)0x80001000

echo \n--- After Read Watchpoint Hit ---\n
info registers t1 t3

echo Setting access watchpoint on 0x80001004...\n
delete watchpoints
break *0x80000028
continue
delete breakpoints
awatch *(int*)0x80001004

echo \n--- After Access Watchpoint Hit ---\n
info registers t1 t2
x/2w 0x80001000

echo \n--- Watchpoint Demo Complete ---\n
echo Memory 0x80001000 was monitored for read/write operations\n
echo Memory 0x80001004 was monitored for any access\n

quit
