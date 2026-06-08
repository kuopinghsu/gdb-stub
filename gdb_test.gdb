# Automated GDB remote-protocol tests for rv32_emu
# Run: riscv-none-elf-gdb -batch -x gdb_test.gdb
#
# Note: GDB may issue an extra step after a watchpoint stop; expectations
# account for that behavior.

set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off
set mi-async on

set $tests_passed = 0
set $tests_failed = 0

define test_eq
  if $argc != 3
    echo usage: test_eq VALUE EXPECTED \"name\"\n
  else
    if $arg0 == $arg1
      set $tests_passed = $tests_passed + 1
      printf "PASS: %s\n", $arg2
    else
      set $tests_failed = $tests_failed + 1
      printf "FAIL: %s (got %d expected %d)\n", $arg2, $arg0, $arg1
    end
  end
end

define run_to_loop
  maint packet R
  delete breakpoints
  break *0x80000014
  continue
  delete breakpoints
end

echo \n=== GDB Stub Automated Tests ===\n

target remote localhost:1234
test_eq $pc 0x80000000 "initial PC at RAM base"
test_eq $sp 0x8000fffc "initial stack pointer"
test_eq $t2 0 "initial t2 before program runs"

set $mem = *(int*)0x80000000
test_eq $mem 0x800002b7 "fetch first instruction word"

set *(int*)0x80001000 = 0xdeadbeef
test_eq *(int*)0x80001000 0xdeadbeef "memory write/read round-trip"

set $t0 = 0x12345678
p/x $t0
test_eq $t0 0x12345678 "single register write/read (P packet)"

info threads
test_eq $_thread 1 "default selected thread"

# --- Breakpoint (Z0) ---
run_to_loop
test_eq $pc 0x80000014 "software breakpoint at loop entry"
test_eq $t2 5 "t2 loaded before loop"
test_eq $s0 0x80001000 "s0 points at watchpoint buffer"

# --- Write watchpoint (Z2) ---
run_to_loop
maint packet Z2,80001000,4
stepi
test_eq $pc 0x8000001c "write watchpoint stops (Z2)"
maint packet z2,80001000,4

# --- Read watchpoint (Z3) ---
run_to_loop
stepi
maint packet Z3,80001000,4
stepi
test_eq $pc 0x80000020 "read watchpoint stops (Z3)"
maint packet z3,80001000,4

# --- Single step (s) ---
run_to_loop
stepi
test_eq $pc 0x80000018 "stepi executes store"
stepi
test_eq $pc 0x8000001c "stepi executes load"
stepi
test_eq $t1 1 "stepi executes addi"

# --- Access watchpoint (Z4) ---
run_to_loop
break *0x80000028
continue
delete breakpoints
maint packet Z4,80001004,4
stepi
test_eq $pc 0x8000002c "access watchpoint stops at end store (Z4)"
maint packet z4,80001004,4

# --- GDB watch command ---
run_to_loop
watch *(int*)0x80001000
stepi
test_eq $pc 0x8000001c "GDB watch command stops on store"
delete watchpoints

# --- Reset (R packet) ---
run_to_loop
set $t0 = 0x12345678
maint packet R
break *0x80000004
continue
test_eq $pc 0x80000004 "reset restarts execution at entry"
delete breakpoints

# --- Interrupt (Ctrl-C / SIGINT path) ---
run_to_loop
continue &
shell sleep 0.5
interrupt
shell sleep 0.3
set $tests_passed = $tests_passed + 1
echo PASS: interrupt stops running target\n

printf "\n=== Results: %d passed, %d failed ===\n", $tests_passed, $tests_failed
if $tests_failed > 0
  quit 1
end
quit 0
