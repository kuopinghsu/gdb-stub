#ifndef RV32_CPU_H
#define RV32_CPU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// RISC-V RV32I CPU emulator for GDB stub demonstration

// CPU registers
#define RV32_REG_COUNT 32
#define RV32_REG_ZERO  0
#define RV32_REG_RA    1
#define RV32_REG_SP    2
#define RV32_REG_GP    3
#define RV32_REG_TP    4
#define RV32_REG_T0    5
#define RV32_REG_T1    6
#define RV32_REG_T2    7
#define RV32_REG_S0    8
#define RV32_REG_S1    9
#define RV32_REG_S2    18
#define RV32_REG_A0    10
#define RV32_REG_A1    11

// Memory configuration
#define RAM_SIZE    (64 * 1024)  // 64KB RAM
#define RAM_BASE    0x80000000
#define UART_BASE   0x10000000
#define IDLE_LOOP_ADDR      0x8000002c
#define RV32_MAX_HARTS      8

typedef bool (*rv32_watchpoint_check_fn)(void *ctx, uint32_t addr, uint32_t len);

// CPU state
typedef struct {
    uint32_t regs[RV32_REG_COUNT];
    uint32_t pc;
    uint8_t  memory[RAM_SIZE];
    uint8_t *shared_ram;            // Shared RAM for SMP (NULL = use memory[])
    int      hart_id;               // 0-based hart index
    bool     running;
    bool     single_step_mode;
    bool     halted;                // CPU halted due to watchpoint or breakpoint
    uint64_t instruction_count;
    void *watchpoint_ctx;
    rv32_watchpoint_check_fn check_watchpoint_read;
    rv32_watchpoint_check_fn check_watchpoint_write;
} rv32_cpu_t;

// Instruction formats
typedef struct {
    uint32_t opcode : 7;
    uint32_t rd     : 5;
    uint32_t funct3 : 3;
    uint32_t rs1    : 5;
    uint32_t rs2    : 5;
    uint32_t funct7 : 7;
} r_type_t;

typedef struct {
    uint32_t opcode : 7;
    uint32_t rd     : 5;
    uint32_t funct3 : 3;
    uint32_t rs1    : 5;
    uint32_t imm    : 12;
} i_type_t;

typedef struct {
    uint32_t opcode : 7;
    uint32_t imm_0  : 5;
    uint32_t funct3 : 3;
    uint32_t rs1    : 5;
    uint32_t rs2    : 5;
    uint32_t imm_5  : 7;
} s_type_t;

// RISC-V opcodes
#define OPCODE_LUI      0x37
#define OPCODE_AUIPC    0x17
#define OPCODE_JAL      0x6F
#define OPCODE_JALR     0x67
#define OPCODE_BRANCH   0x63
#define OPCODE_LOAD     0x03
#define OPCODE_STORE    0x23
#define OPCODE_OP_IMM   0x13
#define OPCODE_OP       0x33
#define OPCODE_SYSTEM   0x73

// Function declarations
void rv32_cpu_init(rv32_cpu_t *cpu);
void rv32_cpu_reset(rv32_cpu_t *cpu);
void rv32_cpu_step(rv32_cpu_t *cpu);
bool rv32_cpu_load_program(rv32_cpu_t *cpu, const uint8_t *program, size_t size, uint32_t base_addr);

// Memory access functions
uint32_t rv32_cpu_read_mem(rv32_cpu_t *cpu, uint32_t addr, int size);
void rv32_cpu_write_mem(rv32_cpu_t *cpu, uint32_t addr, uint32_t value, int size);

// Register access
uint32_t rv32_cpu_read_reg(rv32_cpu_t *cpu, int reg);
void rv32_cpu_write_reg(rv32_cpu_t *cpu, int reg, uint32_t value);

// Debug helpers
void rv32_cpu_print_state(rv32_cpu_t *cpu);
void rv32_cpu_disassemble(uint32_t instruction, uint32_t pc, char *buffer, size_t buf_size);

#endif // RV32_CPU_H