# Automated SMP GDB remote-protocol tests
# Usage: riscv-none-elf-gdb -batch -ex "set $expected_harts=4" -x gdb_test_smp.gdb
# $expected_harts must be set to match emulator -c (2, 4, or 8)

set confirm off
set pagination off
set print pretty off
set mem inaccessible-by-default off

set $tests_passed = 0
set $tests_failed = 0
set $last_thread = $expected_harts
set $last_tp = $expected_harts - 1
set $worker_pc = 0x80000034
set $worker_step_pc = 0x80000038

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

define test_ge
  if $argc != 3
    echo usage: test_ge VALUE MINIMUM \"name\"\n
  else
    if $arg0 >= $arg1
      set $tests_passed = $tests_passed + 1
      printf "PASS: %s\n", $arg2
    else
      set $tests_failed = $tests_failed + 1
      printf "FAIL: %s (got %d expected >= %d)\n", $arg2, $arg0, $arg1
    end
  end
end

define run_to_loop
  maint packet R
  delete breakpoints
  thread 1
  break *0x80000014
  continue
  delete breakpoints
end

define run_one_loop_store
  maint packet R
  delete breakpoints
  thread 1
  break *0x80000014
  continue
  stepi
  delete breakpoints
end

printf "\n=== GDB Stub SMP Tests (%d harts) ===\n", $expected_harts

target remote localhost:1234

info threads
test_eq $_thread 1 "default selected thread is hart 0"

thread 1
test_eq $tp 0 "hart 0 tp register"
test_eq $pc 0x80000000 "hart 0 starts at entry"

thread 2
test_eq $tp 1 "hart 1 tp register"
test_eq $pc $worker_pc "hart 1 parked at worker poll loop"

thread $last_thread
test_eq $tp $last_tp "last hart tp register"
test_eq $pc $worker_pc "last hart parked at worker poll loop"

thread 1
run_to_loop
test_eq $pc 0x80000014 "hart 0 breakpoint at loop entry"

# IPI + shared sync: one loop store dispatches work to a worker hart
run_one_loop_store
set $done = *(int*)0x8000200c
test_ge $done 1 "shared done counter after IPI/work dispatch"
thread 2
test_ge $s2 1 "worker hart s2 records completed jobs"

# Per-hart continue: only thread 2 (hart 1) should run
thread 1
test_eq $pc 0x80000018 "hart 0 still at loop after worker step"
thread 2
maint packet Hcp1.2
stepi
test_eq $pc $worker_step_pc "hart 1 stepped in worker poll loop (Hcp1.2)"
thread 1
test_eq $pc 0x80000018 "hart 0 unchanged while only hart 1 ran"
maint packet Hcp1.-1

# Breakpoint stop reports correct thread
thread 1
run_to_loop
test_eq $_thread 1 "breakpoint stop on hart 0 thread id"

# Reset restores all harts
maint packet R
thread 1
break *0x80000004
continue
test_eq $pc 0x80000004 "reset restores hart 0 entry"
delete breakpoints
thread 2
test_eq $pc $worker_pc "reset restores secondary hart worker poll PC"
test_eq $tp 1 "reset preserves hart id in tp"
test_eq *(int*)0x8000200c 0 "reset clears shared done counter"

# Watchpoint still works on hart 0 with SMP enabled
thread 1
run_to_loop
maint packet Z2,80001000,4
stepi
test_eq $pc 0x8000001c "write watchpoint on hart 0 (Z2)"
maint packet z2,80001000,4

printf "\n=== SMP Results (%d harts): %d passed, %d failed ===\n", $expected_harts, $tests_passed, $tests_failed
if $tests_failed > 0
  quit 1
end
quit 0
