# GDB Script for RISC-V Emulator Demo
# Load this script with: gdb -x gdb_demo.gdb
#
# This demo showcases:
# - Basic debugging (breakpoints, stepping)
# - Memory examination
# - Watchpoint functionality (read/write/access monitoring)

set confirm off
set pagination off

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

# Set up watchpoints for memory location 0x80001000
echo \n--- Setting Up Watchpoints ---\n
echo Setting write watchpoint on 0x80001000...\n
watch *(int*)0x80001000

echo Setting read watchpoint on 0x80001000...\n
rwatch *(int*)0x80001000

echo Setting access watchpoint on 0x80001004...\n
awatch *(int*)0x80001004

# Show all watchpoints
info watchpoints

# Continue execution - should hit write watchpoint
echo \n--- Continuing - Should Hit Write Watchpoint ---\n
continue

# Show what happened
echo \n--- After Write Watchpoint Hit ---\n
info registers t1 t3
x/2w 0x80001000

# Continue execution - should hit read watchpoint
echo \n--- Continuing - Should Hit Read Watchpoint ---\n
continue

# Show registers after read
echo \n--- After Read Watchpoint Hit ---\n
info registers t1 t3

# Continue through several iterations to see watchpoints in action
echo \n--- Continuing Through Loop Iterations ---\n
continue

# Show final memory state
echo \n--- Final Memory State ---\n
x/4w 0x80001000

# Continue to end of program
echo \n--- Continuing to Program End ---\n
continue

# Show final state
echo \n--- Final Program State ---\n
info registers t1 t2
x/2w 0x80001000

echo \n--- Watchpoint Demo Complete ---\n
echo Memory 0x80001000 was monitored for read/write operations\n
echo Memory 0x80001004 was monitored for any access\n

quit

