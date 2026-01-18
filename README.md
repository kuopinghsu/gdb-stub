# RISC-V Emulator with GDB Remote Serial Protocol Stub

A complete implementation of a RISC-V RV32I emulator with integrated GDB stub support, demonstrating how to implement the GDB Remote Serial Protocol for debugging embedded systems and simulators.

## Features

- **Complete RISC-V RV32I CPU Emulator**
  - 32-bit RISC-V instruction set architecture
  - 32 general-purpose registers (x0-x31)
  - 64KB RAM memory space
  - Basic UART device simulation
  - Support for all RV32I base instructions

- **Full GDB Remote Serial Protocol Support**
  - Breakpoint management (software breakpoints)
  - Watchpoint support (read/write/access)
  - Memory and register inspection/modification
  - Single-step debugging
  - Continue/halt execution control
  - Thread management (single-threaded)
  - Reset and restart capabilities

- **Cross-Platform Compatibility**
  - Compiles with GCC on Linux, macOS, and Windows
  - No external dependencies beyond standard C library
  - Socket-based communication with GDB

## Quick Start

### Building

```bash
make all
```

### Running the Demo

```bash
# Complete demo - builds, starts emulator, and launches GDB
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

# Start emulator in background for manual GDB connection
make start-emu

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
# Or manually:
riscv-none-elf-gdb
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
│   └── main.c              # Main program
├── build/                  # Build artifacts
├── examples/               # Example programs
├── Makefile               # Build system
└── README.md              # This file
```

### Memory Map

| Address Range    | Description          | Size  |
|------------------|---------------------|-------|
| `0x80000000`     | RAM (Program/Data)  | 64KB  |
| `0x10000000`     | UART Device         | 4B    |

### CPU Registers

The emulator implements the standard RISC-V register convention:

| Register | ABI Name | Description               |
|----------|----------|---------------------------|
| x0       | zero     | Hardwired zero           |
| x1       | ra       | Return address           |
| x2       | sp       | Stack pointer            |
| x3       | gp       | Global pointer           |
| x4       | tp       | Thread pointer           |
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
| `qSymbol` | Symbol lookup | ✅ Complete |
| `qAttached` | Query attach status | ✅ Complete |
| `qTStatus` | Trace status | ✅ Complete |
| `qSearch:memory` | Search memory | ✅ Complete |

### Protocol Features

1. **Packet Format**: All GDB packets use the standard `$<data>#<checksum>` format
2. **Checksum Validation**: Full checksum validation with ACK/NACK responses
3. **Error Handling**: Proper error responses for invalid commands
4. **Thread Support**: Single-threaded model with proper thread ID handling
5. **Memory Protection**: Bounds checking for all memory accesses
6. **Register Mapping**: Standard RISC-V register layout with PC as register 32

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
# Quick demo (recommended)
$ make demo

# Or manual approach:
# Terminal 1: Start emulator
$ make start-emu
RISC-V RV32I Emulator with GDB Stub
===================================
Test program loaded at 0x80000000
GDB stub listening on port 1234
Waiting for GDB connection...

# Terminal 2: Connect with GDB
$ make gdb-connect
(gdb) target remote localhost:1234
Remote debugging using localhost:1234
(gdb) info registers
x0             0x0      0
x1             0x0      0
x2             0x8000fffc       -2147418116
...
pc             0x80000000       0x80000000

(gdb) x/10i $pc
=> 0x80000000:  lui     t0,0x80000
   0x80000004:  addi    t1,zero,0
   0x80000008:  addi    t2,zero,10
   0x8000000c:  lui     s0,0x10000
   0x80000010:  beq     t1,t2,0x80000020

(gdb) break *0x80000010
Breakpoint 1 at 0x80000010
(gdb) continue
Continuing.

Breakpoint 1, 0x80000010 in ?? ()
(gdb) info registers t1 t2
t1             0x0      0
t2             0xa      10
```

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

```bash
# Build the emulator
make all

# Run complete demo (clean, build, start emulator, launch GDB)
make demo

# Start emulator in background
make start-emu

# Connect to running emulator with GDB
make gdb-connect

# Stop background emulator
make stop-emu

# Run with various options
make run           # Default settings
make debug         # Debug output enabled
make interactive   # Interactive mode

# Utility targets
make clean         # Remove build artifacts
make help          # Show all available targets
```

### Direct Command Line Usage

You can still run the emulator directly if needed:

```bash
# Run on different port
./build/rv32_emu -p 2345

# Debug mode with verbose output
./build/rv32_emu -d

# Interactive mode for step-by-step execution
./build/rv32_emu -i
```

## Implementation Details

### GDB Stub Architecture

The GDB stub is implemented as a separate module (`gdb_stub.c/h`) that communicates with the emulator through callback functions. This design allows easy integration with any simulator or emulator.

#### Key Components:

1. **Socket Server**: Listens for GDB connections on specified port
2. **Packet Parser**: Handles GDB Remote Serial Protocol packet format
3. **Command Dispatcher**: Routes commands to appropriate handlers
4. **State Management**: Tracks breakpoints, watchpoints, and execution state
5. **Callback Interface**: Clean separation between GDB stub and target system

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
} gdb_callbacks_t;
```

### CPU Emulator Architecture

The RISC-V CPU emulator implements a simple fetch-decode-execute cycle with full support for the RV32I instruction set.

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

### Built-in Test Program

The emulator includes a simple test program that demonstrates basic functionality:

1. Load immediate values into registers
2. Perform arithmetic operations
3. Implement a counting loop
4. Use conditional branches
5. System call to terminate

### GDB Integration Testing

The quickest way to test all functionality is with the demo:

```bash
# Complete integrated test
make demo
```

For manual testing of specific GDB functionality:

```bash
# Start emulator
make start-emu

# In another terminal, test functionality
make gdb-connect
# Or connect manually:
riscv-none-elf-gdb
```

Test all major GDB functionality:

```bash
# Test basic connection
(gdb) target remote localhost:1234

# Test register access
(gdb) info registers
(gdb) set $t0 = 0x12345678

# Test memory access
(gdb) x/10i $pc
(gdb) x/10w 0x80000000

# Test breakpoints
(gdb) break *0x80000010
(gdb) continue

# Test single stepping
(gdb) stepi

# Test watchpoints
(gdb) watch *(int*)0x80001000
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

1. **GDB Connection Refused**
   - Check if port is already in use: `netstat -ln | grep 1234`
   - Stop any running emulator: `make stop-emu`
   - Try different port: `./build/rv32_emu -p 2345`

2. **Breakpoints Not Working**
   - Ensure address is valid: `(gdb) info mem`
   - Check breakpoint list: `(gdb) info breakpoints`

3. **Memory Access Errors**
   - Verify address is within valid range (0x80000000-0x8000FFFF)
   - Check alignment for multi-byte accesses

4. **Program Not Executing**
   - Verify program was loaded correctly
   - Check PC is set to entry point: `(gdb) info registers pc`

### Debug Options

```bash
# Run complete demo with all debugging features
make demo

# Enable verbose debug output
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

1. Additional RISC-V instruction extensions (M, A, F, D)
2. More sophisticated memory management
3. Additional peripheral devices
4. Performance optimizations
5. Enhanced debugging features

## References

- [RISC-V Instruction Set Manual](https://riscv.org/specifications/)
- [GDB Remote Serial Protocol Documentation](https://sourceware.org/gdb/current/onlinedocs/gdb/Remote-Protocol.html)
- [RISC-V Software Toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain)
