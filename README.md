# RISC-V Emulator with GDB Remote Serial Protocol Stub

A complete implementation of a RISC-V RV32I emulator with integrated GDB stub support, demonstrating how to implement the GDB Remote Serial Protocol for debugging embedded systems and simulators.

## Prerequisites

- GCC (C99)
- `riscv-none-elf-gdb` (or another RISC-V GDB build with remote debugging support)

## Features

- **Complete RISC-V RV32I CPU Emulator**
  - 32-bit RISC-V instruction set architecture
  - 32 general-purpose registers (x0-x31)
  - 64KB RAM memory space
  - Basic UART device simulation
  - Support for all RV32I base instructions

- **Full GDB Remote Serial Protocol Support**
  - Software breakpoints and read/write/access watchpoints
  - Memory and register inspection/modification (`g`/`G`/`m`/`M`/`p`/`P`/`X`)
  - Single-step and continue/halt control (`s`/`c` and `vCont`)
  - Multiprocess stop replies (`thread:p1.N`) with `swbreak:`, `watch:`, and PC (`20:`) fields
  - SMP-ready thread protocol (`H`/`T`/`qC`/`qfThreadInfo`; `-c 1`, `2`, `4`, or `8`)
  - Worker harts run RV32I poll/handler code (`emu/smp_program.h`); IPI + spinlock-protected shared queue
  - Target reset (`R`) and Ctrl-C interrupt handling

- **Automated test suite** — 21 single-core + 19 SMP checks each for 2, 4, and 8 harts via `make test`

- **Cross-Platform Compatibility**
  - Compiles with GCC on Linux, macOS, and Windows
  - No external dependencies beyond standard C library
  - Socket-based communication with GDB

## Quick Start

### Building

```bash
make all
```

### Running Tests

```bash
# Run single-core + SMP (2, 4, 8 hart) automated checks
make test

# SMP tests only (2, 4, and 8 harts)
make test-smp
```

### Running the Demo

```bash
# Build, run all tests, single-core demo, SMP tests (2/4/8), and SMP demo
make demo
```

### Manual Usage

```bash
# Build and run emulator with default settings (GDB server on port 1234)
make run

# Run with debug output
make debug

# Run in interactive mode
make interactive

# Start emulator in background (default: 1 hart; use HARTS=2|4|8 for SMP)
make start-emu
make start-emu HARTS=8

# Connect to running emulator with GDB
make gdb-connect

# Stop background emulator
make stop-emu
```

### Manual GDB Connection

1. Start the emulator:
```bash
make start-emu
```

2. In another terminal, connect with GDB:
```bash
make gdb-connect
# Or manually (recommended settings):
riscv-none-elf-gdb examples/test.s
(gdb) set mem inaccessible-by-default off
(gdb) target remote localhost:1234
(gdb) continue
```

## Architecture Overview

### Directory Structure

```
gdb-stub/
├── src/
│   ├── gdb_stub.h          # GDB stub interface
│   └── gdb_stub.c          # GDB stub implementation
├── emu/
│   ├── rv32_cpu.h          # CPU emulator interface
│   ├── rv32_cpu.c          # CPU emulator implementation
│   ├── rv32_system.h       # Multi-hart system state
│   ├── smp_sync.h          # IPI + shared synchronization
│   ├── smp_sync.c
│   ├── smp_program.h       # Worker hart RV32I poll/handler image
│   └── main.c              # Main program + GDB callbacks
├── gdb_test.gdb            # Single-core automated tests (-c 1)
├── gdb_test_smp.gdb        # SMP automated tests (-c 2/4/8; set $expected_harts)
├── gdb_demo.gdb            # Single-core watchpoint demo
├── gdb_demo_smp.gdb        # SMP demo (set $expected_harts to match -c)
├── build/                  # Build artifacts
├── examples/
│   └── test.s              # Assembly source (symbols for manual GDB sessions)
├── Makefile                # Build system
└── README.md               # This file
```

### Memory Map

| Address Range    | Description          | Size  |
|------------------|---------------------|-------|
| `0x80000000`     | RAM (Program/Data)  | 64KB  |
| `0x80002000`     | SMP sync region     | 64B   |
| `0x10000000`     | UART device         | 4B    |
| `0x10000004`     | IPI send MMIO       | 4B    |

### CPU Registers

The emulator implements the standard RISC-V register convention:

| Register | ABI Name | Description               |
|----------|----------|---------------------------|
| x0       | zero     | Hardwired zero           |
| x1       | ra       | Return address           |
| x2       | sp       | Stack pointer            |
| x3       | gp       | Global pointer           |
| x4       | tp       | Thread pointer (holds 0-based hart id in SMP mode) |
| x5-x7    | t0-t2    | Temporaries              |
| x8       | s0/fp    | Saved register/frame ptr |
| x9       | s1       | Saved register           |
| x10-x11  | a0-a1    | Function arguments       |
| x12-x17  | a2-a7    | Function arguments       |
| x18-x27  | s2-s11   | Saved registers          |
| x28-x31  | t3-t6    | Temporaries              |

## GDB Stub Implementation Details

### Supported GDB Remote Protocol Commands

| Command | Description | Implementation Status |
|---------|-------------|----------------------|
| `?` | Halt reason | ✅ Complete |
| `g` | Read all registers | ✅ Complete |
| `G` | Write all registers | ✅ Complete |
| `p` | Read single register | ✅ Complete |
| `P` | Write single register | ✅ Complete |
| `m` | Read memory | ✅ Complete |
| `M` | Write memory | ✅ Complete |
| `X` | Write memory (binary) | ✅ Complete |
| `c` | Continue execution | ✅ Complete |
| `s` | Single step | ✅ Complete |
| `Z0` | Insert software breakpoint | ✅ Complete |
| `z0` | Remove software breakpoint | ✅ Complete |
| `Z2` | Insert write watchpoint | ✅ Complete |
| `z2` | Remove write watchpoint | ✅ Complete |
| `Z3` | Insert read watchpoint | ✅ Complete |
| `z3` | Remove read watchpoint | ✅ Complete |
| `Z4` | Insert access watchpoint | ✅ Complete |
| `z4` | Remove access watchpoint | ✅ Complete |
| `H` | Set thread for operations | ✅ Complete |
| `T` | Check if thread is alive | ✅ Complete |
| `k` | Kill program | ✅ Complete |
| `D` | Detach debugger | ✅ Complete |
| `R` | Restart program | ✅ Complete |
| `qSupported` | Query supported features | ✅ Complete |
| `qC` | Get current thread | ✅ Complete |
| `qOffsets` | Get section offsets | ✅ Complete |
| `qAttached` | Query attach status | ✅ Complete |
| `qTStatus` | Trace status | ✅ Complete |
| `qSearch:memory` | Search memory | ✅ Complete |
| `qfThreadInfo` | List threads (first) | ✅ Complete |
| `qsThreadInfo` | List threads (next) | ✅ Complete |
| `vCont` | Continue/step with thread id (`p pid.tid`) | ✅ Complete |
| `vCont?` | Query supported vCont actions | ✅ Complete |

### Protocol Features

1. **Packet format**: Standard `$<data>#<checksum>` with ACK/NACK and full write retries
2. **Register encoding**: Little-endian hex for `g`/`G`/`p`/`P` (RISC-V target byte order)
3. **Stop replies**: `T` packets with multiprocess `thread:p1.N`, plus `swbreak:`, `watch:`, and PC (`20:`) fields
4. **Thread support**: SMP callbacks (`get_num_harts`, `set_focus_hart`, `halt_cpus`, …); emulator supports 1, 2, 4, or 8 harts via `-c`
5. **vCont**: Extended continue/step (`vCont;s:p1.2`, `vCont;c:p1.-1`, …) with `vCont?` capability query
6. **Multiprocess ids**: Process id `1` for all harts; thread ids `p1.1`…`p1.N` in `H`, `T`, `qC`, `qfThreadInfo`, and stop replies (negotiated via `multiprocess+` in `qSupported`; preserved across `R` reset)
7. **vCont step priority**: Combined packets such as `vCont;s:p1.1;c:p1.-1` treat the step action as authoritative (GDB sends this after breakpoint stops)
8. **Binary writes**: `X` command with RSP escape and run-length decoding
9. **Memory protection**: Bounds checking on all CPU and stub memory accesses
10. **Register layout**: x0–x31 plus PC as register 32

### Breakpoint Management

- **Software Breakpoints**: Up to 64 concurrent breakpoints
- **Watchpoints**: Up to 32 watchpoints with read/write/access types
- **Address Validation**: All breakpoint addresses validated against memory map
- **Efficient Checking**: O(1) breakpoint checking during execution

### Memory Management

- **Virtual Memory**: Simple flat memory model
- **Endianness**: Little-endian byte order (RISC-V standard)
- **Access Sizes**: Support for 8-bit, 16-bit, and 32-bit accesses
- **Memory Mapped I/O**: UART device at `0x10000000`

## Usage Examples

### Basic Debugging Session

```bash
make test          # 21 single-core + 19×3 SMP checks
make demo          # Full test + demo pipeline

# Manual workflow (single core)
make start-emu
make gdb-connect

# Manual workflow (8 hart SMP)
make start-emu HARTS=8
riscv-none-elf-gdb -ex 'set $expected_harts=8' -x gdb_demo_smp.gdb examples/test.s
```

Example session after connecting:

```gdb
(gdb) set mem inaccessible-by-default off
(gdb) target remote localhost:1234
(gdb) info registers t1 t2 s0
t1             0x0      0
t2             0x5      5
s0             0x80001000
(gdb) break *0x80000014
(gdb) continue
Breakpoint 1, 0x80000014 in loop ()
(gdb) stepi
(gdb) info registers t1
t1             0x1      1
```

### SMP Debugging

With `-c 2`, `4`, or `8`, GDB exposes one thread per hart. Hart 0 runs the demo loop; worker harts park in an RV32I poll/handler loop starting at `0x80000034` (`worker_poll`). Each loop store on hart 0 writes an IPI to shared RAM; the target worker runs handler code at `0x80000054` under a spinlock.

```gdb
(gdb) info threads
  Id   Target Id         Frame
* 1    Thread 1          0x80000000 in ?? ()
  2    Thread 2          0x80000034 in ?? ()
  ...
(gdb) thread 2
(gdb) info registers tp pc s1 s2    # tp == hart id; s2 == jobs completed
(gdb) x/4w 0x80002000               # lock, work_item, work_ready, shared_done
(gdb) thread 1
(gdb) break *0x80000014
(gdb) continue
(gdb) stepi                         # loop store triggers IPI + worker job
(gdb) x/w 0x8000200c                # shared_done >= 1
```

Manual IPI from GDB: `set *(int*)0x10000004 = 0x00010005` sends work `5` to hart 1 (`(1 << 16) | value`). If the high byte is zero, the target hart is chosen automatically from the work value.

Per-hart continue uses `HcpN` / `Hcp1.-1` or `vCont;c:pN` / `vCont;s:pN` (multiprocess thread ids). Normal `continue` runs hart 0 only; worker jobs run in RV32I when hart 0 executes the loop store.

### Setting Watchpoints

```gdb
# Watch for writes to memory location
(gdb) watch *(int*)0x80001000

# Watch for reads from memory location
(gdb) rwatch *(int*)0x80001000

# Watch for any access to memory location
(gdb) awatch *(int*)0x80001000
```

### Memory Examination

```gdb
# Examine memory as instructions
(gdb) x/10i 0x80000000

# Examine memory as 32-bit words
(gdb) x/10w 0x80000000

# Examine memory as bytes
(gdb) x/40b 0x80000000

# Set memory values
(gdb) set *(int*)0x80001000 = 0x12345678
```

### Single Stepping

```gdb
# Step one instruction
(gdb) stepi

# Step multiple instructions
(gdb) stepi 5

# Continue until next breakpoint
(gdb) continue
```

## Available Make Targets

| Target | Description |
|--------|-------------|
| `make all` | Build the emulator (default) |
| `make test` | Clean, build, run single-core + SMP automated tests |
| `make test-smp` | Clean, build, run SMP tests for 2, 4, and 8 harts |
| `make demo` | Clean, build, run all tests then single-core + SMP demos |
| `make run` | Run emulator on port 1234 |
| `make debug` | Run with `-d` (instruction trace) |
| `make interactive` | Run with `-i` (stdin prompts between stops) |
| `make start-emu` | Start emulator in background (`HARTS=1|2|4|8`, default 1) |
| `make gdb-connect` | Connect GDB with demo script |
| `make stop-emu` | Stop background emulator |
| `make clean` | Remove build artifacts |
| `make help` | List all targets |

### Direct Command Line Usage

You can still run the emulator directly if needed:

```bash
# Run on different port
./build/rv32_emu -p 2345

# Single-core (default)
./build/rv32_emu -c 1

# SMP: 2, 4, or 8 harts (hart 0 demo; workers at 0x80000034)
./build/rv32_emu -c 2
./build/rv32_emu -c 4
./build/rv32_emu -c 8

# Debug mode with verbose output
./build/rv32_emu -d

# Interactive mode for step-by-step execution
./build/rv32_emu -i
```

## Implementation Details

### GDB Stub Architecture

The GDB stub is implemented as a separate module (`gdb_stub.c/h`) that communicates with the emulator through callback functions. This design allows easy integration with any simulator or emulator.

#### Key Components:

1. **Socket server**: Listens for GDB on a configurable TCP port
2. **Packet parser**: Checksum validation, Ctrl-C handling, partial-write safety
3. **Command dispatcher**: Routes RSP commands to handlers in `gdb_stub_process()`
4. **State management**: Breakpoints, watchpoints, thread IDs, stop-thread tracking
5. **Callback interface**: Target-specific register/memory/CPU control via `gdb_callbacks_t`
6. **Watchpoint hooks**: CPU calls optional `check_watchpoint_read`/`write` function pointers (no direct stub dependency in `rv32_cpu.c`)

#### Callback Interface:

```c
typedef struct {
    uint32_t (*read_reg)(void *sim, int reg_num);
    void (*write_reg)(void *sim, int reg_num, uint32_t value);
    uint32_t (*read_mem)(void *sim, uint32_t addr, int size);
    void (*write_mem)(void *sim, uint32_t addr, uint32_t value, int size);
    uint32_t (*get_pc)(void *sim);
    void (*set_pc)(void *sim, uint32_t pc);
    void (*single_step)(void *sim);
    bool (*is_running)(void *sim);
    void (*reset)(void *sim);
    void (*resume)(void *sim);
    int (*get_num_harts)(void *sim);       // Optional: number of harts (default 1)
    void (*set_focus_hart)(void *sim, int hart);  // Optional: select hart for reg ops
    int (*get_focus_hart)(void *sim);      // Optional: current focus hart (0-based)
    int (*get_stop_hart)(void *sim);       // Optional: hart that stopped (0-based)
    void (*halt_cpus)(void *sim);          // Optional: halt all harts (Ctrl-C)
} gdb_callbacks_t;
```

When integrating the stub into your own simulator, call `gdb_stub_set_stop_thread()` before sending a stop reply so `T` packets include the correct `thread:p1.N` field. Use `gdb_stub_should_run_thread()` in your run loop if you implement per-hart continue (`continue_thread`).

#### Public helpers:

| Function | Purpose |
|----------|---------|
| `gdb_stub_init` / `gdb_stub_accept` / `gdb_stub_process` | Server lifecycle and command loop |
| `gdb_stub_check_breakpoint` | Test PC against software breakpoints |
| `gdb_stub_check_watchpoint_read` / `write` | Test memory access against watchpoints |
| `gdb_stub_send_stop_signal` / `send_stop_reason` | Notify GDB of a halt |
| `gdb_stub_set_stop_thread` | Set 1-based thread id for the next stop reply |
| `gdb_stub_should_run_thread` | Respect `continue_thread` when resuming |

### CPU Emulator Architecture

The RISC-V CPU emulator implements a simple fetch-decode-execute cycle with full support for the RV32I instruction set. In SMP mode, up to `RV32_MAX_HARTS` (8) hart contexts share one RAM image via `shared_ram`; each hart has its own registers and PC. Worker coordination is handled by `smp_sync.c` (IPI delivery, job stepping) and `smp_program.h` (embedded worker RV32I code).

#### SMP execution model

| Mode | Behavior |
|------|----------|
| `-c 1` | Single hart; standard continue/step |
| `-c 2/4/8` | Hart 0 runs the demo loop; harts 1…N−1 poll at `0x80000034`, run RV32I handler at `0x80000054` on IPI |
| Loop store at `0x80000014` | Publishes work + IPI; worker hart executes handler, increments `shared_done` |
| `0x10000004` IPI MMIO | Write `(target_hart << 16) \| work`; target 0 = auto-select worker |
| `continue` | Runs hart 0 only (workers step when IPI pending or mid-handler) |
| `Hcp1.N` / `vCont` | Per-thread continue or step (`p1.N` = process 1, thread N) |
| Breakpoint/watchpoint stop | `T` packet `thread:p1.N` plus `swbreak:` / `watch:` |

#### Shared sync layout (`0x80002000`)

| Offset | Name | Purpose |
|--------|------|---------|
| `+0x00` | `lock` | Spinlock (0 = free, else holder `hart_id + 1`) |
| `+0x04` | `work_item` | Last published work value |
| `+0x08` | `work_ready` | Non-zero when work is pending |
| `+0x0c` | `shared_done` | Completed job counter (visible to GDB) |
| `+0x10` | `ipi_pending[8]` | Per-hart IPI doorbell flags |
| `+0x30` | `num_harts` | Active hart count |

#### Supported Instructions:

- **Integer Computational**: ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI
- **Integer Register-Register**: ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND
- **Control Transfer**: JAL, JALR, BEQ, BNE, BLT, BGE, BLTU, BGEU
- **Load/Store**: LB, LH, LW, LBU, LHU, SB, SH, SW
- **Upper Immediate**: LUI, AUIPC
- **System**: ECALL, EBREAK

#### Instruction Decode:

The emulator uses bit manipulation to decode instructions according to RISC-V specification:

```c
uint32_t opcode = instruction & 0x7F;
uint32_t rd = (instruction >> 7) & 0x1F;
uint32_t funct3 = (instruction >> 12) & 0x7;
uint32_t rs1 = (instruction >> 15) & 0x1F;
uint32_t rs2 = (instruction >> 20) & 0x1F;
uint32_t funct7 = (instruction >> 25) & 0x7F;
```

## Testing and Validation

### Automated tests (`make test`)

`make test` runs two suites:

1. **`gdb_test.gdb`** with `-c 1` — 21 single-core checks
2. **`gdb_test_smp.gdb`** with `-c 2`, `4`, and `8` — 19 checks each

Both exit with code 0 on success, 1 on failure (suitable for CI).

#### Single-core tests (`gdb_test.gdb`)

| # | Area | What is checked |
|---|------|-----------------|
| 1–3 | Connection | Initial PC, SP, t2 |
| 4–5 | Memory `m`/`M` | Instruction fetch; write/read round-trip |
| 6 | Register `P`/`p` | Single-register write/read (little-endian) |
| 7 | Threads | Default thread id 1 |
| 8–10 | Breakpoint `Z0` | Stop at loop; t2 and s0 values |
| 11 | Watchpoint `Z2` | Write watch stops execution |
| 12 | Watchpoint `Z3` | Read watch stops execution |
| 13–15 | Step `s` / `vCont` | Store, load, addi |
| 16–17 | Multiprocess | `Hcp1.1` step; `Hgp1.1` / `Tp1.1` thread select |
| 18 | Watchpoint `Z4` | Access watch at end store |
| 19 | GDB `watch` | High-level watch command |
| 20 | Reset `R` | CPU restarts at entry after reset |
| 21 | Interrupt | Ctrl-C stops a running target |

#### SMP tests (`gdb_test_smp.gdb`)

Pass `-ex 'set $expected_harts=N'` to match the emulator `-c N` value (2, 4, or 8).

| # | Area | What is checked |
|---|------|-----------------|
| 1 | Threads | Default thread; hart 0 at entry |
| 2–3 | Worker harts | `tp` and worker poll PC (`0x80000034`) for harts 1 and N−1 |
| 4 | Breakpoint | Hart 0 stops at loop |
| 5–6 | IPI + RV32I worker | Loop store increments `shared_done`; worker `s2` updated |
| 7–9 | Per-hart continue | `Hcp1.2` runs only thread 2; hart 0 unchanged |
| 10 | Stop reply | Breakpoint reports thread 1 |
| 11–13 | Reset `R` | All harts restored; `shared_done` cleared |
| 14 | Watchpoint `Z2` | Write watch on hart 0 with SMP enabled |

```bash
make test
# === Results: 21 passed, 0 failed ===       (single-core)
# === SMP Results (2 harts): 19 passed, 0 failed ===
# === SMP Results (4 harts): 19 passed, 0 failed ===
# === SMP Results (8 harts): 19 passed, 0 failed ===
```

The scripts set `set mem inaccessible-by-default off` (required for bare-metal remote targets). The single-core script also sets `set mi-async on` (for the interrupt test).

### Demo (`make demo`)

Runs, in order:

1. Single-core tests (`gdb_test.gdb`)
2. Single-core watchpoint demo (`gdb_demo.gdb`)
3. SMP tests for 2, 4, and 8 harts (`gdb_test_smp.gdb`)
4. SMP watchpoint demo (`gdb_demo_smp.gdb`, 4 harts)

### Built-in test program

The emulator embeds a small loop at `0x80000000` (see also `examples/test.s` for the same program as assembly). It:

1. Initializes registers and a watchpoint buffer at `0x80001000`
2. Counts from 0 to 5 with stores, loads, and branches
3. Stores a final value at `0x80001004` and spins at the end

| Address | Label | Instruction |
|---------|-------|-------------|
| `0x80000000` | `_start` | Setup: `t1=0`, `t2=5`, `s0=0x80001000` |
| `0x80000014` | `loop` | Store/load counter, increment, branch |
| `0x80000028` | `end` | Final store to `0x80001004` |
| `0x8000002c` | `end+4` | Infinite loop after final store |
| `0x80000030` | `worker_wait` | Worker setup (`lui s0`, sync base) |
| `0x80000034` | `worker_poll` | Poll `ipi_pending[tp]`, dispatch to handler |
| `0x80000050` | `worker_spin` | Jump back to poll when no IPI |
| `0x80000054` | `worker_handle` | Spinlock, consume work, increment `shared_done` |
| `0x80000088` | `worker_release` | Release lock, return to poll |

In SMP mode (`-c 2`, `4`, or `8`), hart 0 runs the demo program and dispatches work from the loop store. Worker harts start at `worker_poll` with `s0 = 0x80002000`; handler code in `emu/smp_program.h` processes jobs when an IPI arrives. Use `thread N` in GDB to switch harts; `tp` (x4) holds the 0-based hart id; worker `s1`/`s2` hold the last work item and completed job count.

`make gdb-connect` loads `examples/test.s` so GDB can show labels; the running code comes from the emulator’s embedded image, not from assembling that file.

### Manual GDB session

```bash
make start-emu HARTS=4
riscv-none-elf-gdb -batch -ex 'set $expected_harts=4' -x gdb_demo_smp.gdb examples/test.s
# or single-core:
make start-emu
riscv-none-elf-gdb -batch -x gdb_demo.gdb examples/test.s
# or interactively:
riscv-none-elf-gdb examples/test.s
(gdb) set mem inaccessible-by-default off
(gdb) target remote localhost:1234
(gdb) break *0x80000014
(gdb) continue
```

## Extending the Implementation

### Adding New Instructions

1. Add opcode definition to `rv32_cpu.h`
2. Implement decode and execute logic in `rv32_cpu_step()`
3. Update disassembler in `rv32_cpu_disassemble()`

### Adding New GDB Commands

1. Add command handler function in `gdb_stub.c`
2. Register handler in `gdb_stub_process()` switch statement
3. Update feature list in `handle_query()` if needed

### Adding New Device

1. Define memory-mapped address range
2. Add handling in `rv32_cpu_read_mem()` and `rv32_cpu_write_mem()`
3. Implement device-specific behavior

## Troubleshooting

### Common Issues

1. **GDB connection refused**
   - Port in use: `netstat -ln | grep 1234` (or `lsof -i :1234`)
   - Stop stale emulator: `make stop-emu`
   - Use another port: `./build/rv32_emu -p 2345`

2. **Cannot access memory at address 0x8000…**
   - For bare-metal remote targets, run `(gdb) set mem inaccessible-by-default off` before reading/writing RAM

3. **Breakpoints or watchpoints not working**
   - Use addresses in RAM (`0x80000000`–`0x8000FFFF`)
   - Check lists: `(gdb) info breakpoints` / `(gdb) info watchpoints`

4. **Stale register values after reset**
   - GDB may cache registers after `R`; continue execution or re-fetch with `maint packet g` to sync

5. **Watchpoint + step/continue behaves unexpectedly**
   - GDB may auto-step after inserting a watchpoint; this is normal GDB behavior, not a stub bug

6. **SMP thread count mismatch**
   - Emulator `-c` must match `$expected_harts` in `gdb_test_smp.gdb` / `gdb_demo_smp.gdb`
   - Use `make start-emu HARTS=8` and `-ex 'set $expected_harts=8'` when connecting manually

7. **Tests hang after watchpoint or breakpoint**
   - Stop stale emulator: `make stop-emu`
   - Modern GDB uses `vCont` with multiprocess ids; the stub must keep `multiprocess+` active after `R` reset and prefer step over continue in combined `vCont` packets

### Debug Options

```bash
# Run automated tests
make test

# Run tests plus interactive demo
make demo

# Enable verbose CPU trace
make debug

# Run in interactive mode for step-by-step debugging
make interactive

# Or run directly for custom options
./build/rv32_emu -d
./build/rv32_emu -i
```

## License

This project is released under the MIT License. See individual source files for copyright information.

## Contributing

Contributions are welcome! Areas for improvement:

1. Additional RISC-V extensions (M, A, F, D) for real atomics
2. More peripheral devices and memory regions
3. Performance optimizations

## References

- [RISC-V Instruction Set Manual](https://riscv.org/specifications/)
- [GDB Remote Serial Protocol Documentation](https://sourceware.org/gdb/current/onlinedocs/gdb/Remote-Protocol.html)
- [RISC-V Software Toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain)
