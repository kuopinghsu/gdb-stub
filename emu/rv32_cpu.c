#include "rv32_cpu.h"
#include "gdb_stub.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External reference to GDB context
extern gdb_context_t *g_gdb_ctx;

#define UART_BASE 0x10000000
void rv32_cpu_init(rv32_cpu_t *cpu) {
    memset(cpu, 0, sizeof(rv32_cpu_t));
    cpu->pc = RAM_BASE;  // Start execution from RAM base
    cpu->regs[RV32_REG_SP] = RAM_BASE + RAM_SIZE - 4;  // Stack pointer at top of RAM
    cpu->running = true;
    cpu->halted = false;
}

// Reset CPU to initial state
void rv32_cpu_reset(rv32_cpu_t *cpu) {
    memset(cpu->regs, 0, sizeof(cpu->regs));
    cpu->pc = RAM_BASE;
    cpu->regs[RV32_REG_SP] = RAM_BASE + RAM_SIZE - 4;
    cpu->running = true;
    cpu->halted = false;
    cpu->single_step_mode = false;
    cpu->instruction_count = 0;
    // Don't clear memory to preserve loaded program
}

// Read memory with proper bounds checking
uint32_t rv32_cpu_read_mem(rv32_cpu_t *cpu, uint32_t addr, int size) {
    // Check for read watchpoints if GDB is connected
    if (g_gdb_ctx && g_gdb_ctx->stub.connected) {
        if (gdb_stub_check_watchpoint_read(g_gdb_ctx, addr, size)) {
            // Watchpoint hit - signal will be sent by GDB stub
            cpu->halted = true;
        }
    }

    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE - size + 1) {
        uint32_t offset = addr - RAM_BASE;
        uint32_t value = 0;

        switch (size) {
        case 1:
            value = cpu->memory[offset];
            break;
        case 2:
            value = *(uint16_t*)&cpu->memory[offset];
            break;
        case 4:
            value = *(uint32_t*)&cpu->memory[offset];
            break;
        default:
            fprintf(stderr, "Invalid memory read size: %d\n", size);
            break;
        }
        return value;
    } else if (addr == UART_BASE) {
        // Simple UART read (always return 0 for now)
        return 0;
    } else if (addr < 0x1000) {
        // Low memory addresses (0x0000 - 0x0FFF) - return 0 silently
        // This is common when GDB probes memory during debugging
        return 0;
    }

    // Only print error for unexpected out-of-bounds addresses
    static uint32_t last_error_addr = 0xFFFFFFFF;
    if (addr != last_error_addr) {
        fprintf(stderr, "Memory read out of bounds: 0x%08x\n", addr);
        last_error_addr = addr;
    }
    return 0;
}

// Write memory with proper bounds checking
void rv32_cpu_write_mem(rv32_cpu_t *cpu, uint32_t addr, uint32_t value, int size) {
    // Check for write watchpoints if GDB is connected
    if (g_gdb_ctx && g_gdb_ctx->stub.connected) {
        if (gdb_stub_check_watchpoint_write(g_gdb_ctx, addr, size)) {
            // Watchpoint hit - signal will be sent by GDB stub
            cpu->halted = true;
        }
    }

    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE - size + 1) {
        uint32_t offset = addr - RAM_BASE;

        switch (size) {
        case 1:
            cpu->memory[offset] = value & 0xFF;
            break;
        case 2:
            *(uint16_t*)&cpu->memory[offset] = value & 0xFFFF;
            break;
        case 4:
            *(uint32_t*)&cpu->memory[offset] = value;
            break;
        default:
            fprintf(stderr, "Invalid memory write size: %d\n", size);
            break;
        }
    } else if (addr == UART_BASE) {
        // Simple UART write - print to stdout
        if (size == 1) {
            putchar(value & 0xFF);
            fflush(stdout);
        }
    } else if (addr < 0x1000) {
        // Low memory addresses (0x0000 - 0x0FFF) - ignore silently
        // This is common when GDB probes memory during debugging
        return;
    } else {
        // Only print error for unexpected out-of-bounds addresses
        static uint32_t last_error_addr = 0xFFFFFFFF;
        if (addr != last_error_addr) {
            fprintf(stderr, "Memory write out of bounds: 0x%08x\n", addr);
            last_error_addr = addr;
        }
    }
}

// Read register (x0 is hardwired to zero)
uint32_t rv32_cpu_read_reg(rv32_cpu_t *cpu, int reg) {
    if (reg == 0) return 0;
    if (reg < 0 || reg >= RV32_REG_COUNT) return 0;
    return cpu->regs[reg];
}

// Write register (x0 writes are ignored)
void rv32_cpu_write_reg(rv32_cpu_t *cpu, int reg, uint32_t value) {
    if (reg == 0) return;  // x0 is hardwired to zero
    if (reg < 0 || reg >= RV32_REG_COUNT) return;
    cpu->regs[reg] = value;
}

// Sign extend immediate values
static int32_t sign_extend(uint32_t value, int bits) {
    int32_t sign_bit = 1 << (bits - 1);
    return (value & sign_bit) ? (int32_t)(value | ~((1U << bits) - 1)) : (int32_t)value;
}

// Execute one instruction
void rv32_cpu_step(rv32_cpu_t *cpu) {
    if (!cpu->running) return;

    // Fetch instruction
    uint32_t instruction = rv32_cpu_read_mem(cpu, cpu->pc, 4);
    uint32_t current_pc = cpu->pc;
    cpu->pc += 4;  // Default: advance to next instruction
    cpu->instruction_count++;

    // Decode instruction
    uint32_t opcode = instruction & 0x7F;

    switch (opcode) {
    case OPCODE_LUI: {  // Load Upper Immediate
        uint32_t rd = (instruction >> 7) & 0x1F;
        uint32_t imm = instruction & 0xFFFFF000;
        rv32_cpu_write_reg(cpu, rd, imm);
        break;
    }

    case OPCODE_AUIPC: {  // Add Upper Immediate to PC
        uint32_t rd = (instruction >> 7) & 0x1F;
        uint32_t imm = instruction & 0xFFFFF000;
        rv32_cpu_write_reg(cpu, rd, current_pc + imm);
        break;
    }

    case OPCODE_JAL: {  // Jump and Link
        uint32_t rd = (instruction >> 7) & 0x1F;
        // Extract immediate (20-bit, word-aligned)
        uint32_t imm = ((instruction >> 31) << 20) |          // bit 20
                       (((instruction >> 12) & 0xFF) << 12) | // bits 19:12
                       (((instruction >> 20) & 0x1) << 11) |  // bit 11
                       (((instruction >> 21) & 0x3FF) << 1);  // bits 10:1
        int32_t offset = sign_extend(imm, 21);
        rv32_cpu_write_reg(cpu, rd, cpu->pc);  // Save return address
        cpu->pc = current_pc + offset;
        break;
    }

    case OPCODE_JALR: {  // Jump and Link Register
        uint32_t rd = (instruction >> 7) & 0x1F;
        uint32_t rs1 = (instruction >> 15) & 0x1F;
        uint32_t imm = (instruction >> 20) & 0xFFF;
        int32_t offset = sign_extend(imm, 12);
        uint32_t target = (rv32_cpu_read_reg(cpu, rs1) + offset) & ~1;
        rv32_cpu_write_reg(cpu, rd, cpu->pc);
        cpu->pc = target;
        break;
    }

    case OPCODE_BRANCH: {  // Branch instructions
        uint32_t rs1 = (instruction >> 15) & 0x1F;
        uint32_t rs2 = (instruction >> 20) & 0x1F;
        uint32_t funct3 = (instruction >> 12) & 0x7;

        // Extract immediate
        uint32_t imm = ((instruction >> 31) << 12) |          // bit 12
                       (((instruction >> 7) & 0x1) << 11) |   // bit 11
                       (((instruction >> 25) & 0x3F) << 5) |  // bits 10:5
                       (((instruction >> 8) & 0xF) << 1);     // bits 4:1
        int32_t offset = sign_extend(imm, 13);

        uint32_t val1 = rv32_cpu_read_reg(cpu, rs1);
        uint32_t val2 = rv32_cpu_read_reg(cpu, rs2);
        bool take_branch = false;

        switch (funct3) {
        case 0x0: take_branch = (val1 == val2); break;           // BEQ
        case 0x1: take_branch = (val1 != val2); break;           // BNE
        case 0x4: take_branch = ((int32_t)val1 < (int32_t)val2); break;  // BLT
        case 0x5: take_branch = ((int32_t)val1 >= (int32_t)val2); break; // BGE
        case 0x6: take_branch = (val1 < val2); break;            // BLTU
        case 0x7: take_branch = (val1 >= val2); break;           // BGEU
        }

        if (take_branch) {
            cpu->pc = current_pc + offset;
        }
        break;
    }

    case OPCODE_LOAD: {  // Load instructions
        uint32_t rd = (instruction >> 7) & 0x1F;
        uint32_t rs1 = (instruction >> 15) & 0x1F;
        uint32_t funct3 = (instruction >> 12) & 0x7;
        uint32_t imm = (instruction >> 20) & 0xFFF;
        int32_t offset = sign_extend(imm, 12);
        uint32_t addr = rv32_cpu_read_reg(cpu, rs1) + offset;

        uint32_t value = 0;
        switch (funct3) {
        case 0x0: value = sign_extend(rv32_cpu_read_mem(cpu, addr, 1), 8); break;   // LB
        case 0x1: value = sign_extend(rv32_cpu_read_mem(cpu, addr, 2), 16); break;  // LH
        case 0x2: value = rv32_cpu_read_mem(cpu, addr, 4); break;                   // LW
        case 0x4: value = rv32_cpu_read_mem(cpu, addr, 1) & 0xFF; break;            // LBU
        case 0x5: value = rv32_cpu_read_mem(cpu, addr, 2) & 0xFFFF; break;          // LHU
        }
        rv32_cpu_write_reg(cpu, rd, value);
        break;
    }

    case OPCODE_STORE: {  // Store instructions
        uint32_t rs1 = (instruction >> 15) & 0x1F;
        uint32_t rs2 = (instruction >> 20) & 0x1F;
        uint32_t funct3 = (instruction >> 12) & 0x7;

        uint32_t imm = ((instruction >> 25) << 5) | ((instruction >> 7) & 0x1F);
        int32_t offset = sign_extend(imm, 12);
        uint32_t addr = rv32_cpu_read_reg(cpu, rs1) + offset;
        uint32_t value = rv32_cpu_read_reg(cpu, rs2);

        switch (funct3) {
        case 0x0: rv32_cpu_write_mem(cpu, addr, value, 1); break;  // SB
        case 0x1: rv32_cpu_write_mem(cpu, addr, value, 2); break;  // SH
        case 0x2: rv32_cpu_write_mem(cpu, addr, value, 4); break;  // SW
        }
        break;
    }

    case OPCODE_OP_IMM: {  // Immediate arithmetic
        uint32_t rd = (instruction >> 7) & 0x1F;
        uint32_t rs1 = (instruction >> 15) & 0x1F;
        uint32_t funct3 = (instruction >> 12) & 0x7;
        uint32_t imm = (instruction >> 20) & 0xFFF;
        int32_t imm_val = sign_extend(imm, 12);
        uint32_t val1 = rv32_cpu_read_reg(cpu, rs1);
        uint32_t result = 0;

        switch (funct3) {
        case 0x0: result = val1 + imm_val; break;                    // ADDI
        case 0x2: result = ((int32_t)val1 < imm_val) ? 1 : 0; break; // SLTI
        case 0x3: result = (val1 < (uint32_t)imm_val) ? 1 : 0; break; // SLTIU
        case 0x4: result = val1 ^ imm_val; break;                     // XORI
        case 0x6: result = val1 | imm_val; break;                     // ORI
        case 0x7: result = val1 & imm_val; break;                     // ANDI
        case 0x1: result = val1 << (imm & 0x1F); break;               // SLLI
        case 0x5:
            if (imm & 0x400) {
                result = (int32_t)val1 >> (imm & 0x1F);  // SRAI
            } else {
                result = val1 >> (imm & 0x1F);           // SRLI
            }
            break;
        }
        rv32_cpu_write_reg(cpu, rd, result);
        break;
    }

    case OPCODE_OP: {  // Register arithmetic
        uint32_t rd = (instruction >> 7) & 0x1F;
        uint32_t rs1 = (instruction >> 15) & 0x1F;
        uint32_t rs2 = (instruction >> 20) & 0x1F;
        uint32_t funct3 = (instruction >> 12) & 0x7;
        uint32_t funct7 = (instruction >> 25) & 0x7F;

        uint32_t val1 = rv32_cpu_read_reg(cpu, rs1);
        uint32_t val2 = rv32_cpu_read_reg(cpu, rs2);
        uint32_t result = 0;

        switch (funct3) {
        case 0x0:
            if (funct7 == 0x20) {
                result = val1 - val2;  // SUB
            } else {
                result = val1 + val2;  // ADD
            }
            break;
        case 0x1: result = val1 << (val2 & 0x1F); break;  // SLL
        case 0x2: result = ((int32_t)val1 < (int32_t)val2) ? 1 : 0; break;  // SLT
        case 0x3: result = (val1 < val2) ? 1 : 0; break;  // SLTU
        case 0x4: result = val1 ^ val2; break;  // XOR
        case 0x5:
            if (funct7 == 0x20) {
                result = (int32_t)val1 >> (val2 & 0x1F);  // SRA
            } else {
                result = val1 >> (val2 & 0x1F);           // SRL
            }
            break;
        case 0x6: result = val1 | val2; break;   // OR
        case 0x7: result = val1 & val2; break;   // AND
        }
        rv32_cpu_write_reg(cpu, rd, result);
        break;
    }

    case OPCODE_SYSTEM: {  // System instructions
        uint32_t funct3 = (instruction >> 12) & 0x7;
        if (funct3 == 0x0) {
            uint32_t imm = (instruction >> 20) & 0xFFF;
            if (imm == 0x0) {
                // ECALL - system call, continue execution for testing
                printf("ECALL executed at PC=0x%08x\n", current_pc);
                // Don't halt CPU to allow forever loop to continue
            } else if (imm == 0x1) {
                // EBREAK - breakpoint
                printf("\nEBREAK executed\n");
                cpu->running = false;
            }
        }
        break;
    }

    default:
        printf("Unknown opcode: 0x%02x at PC=0x%08x\n", opcode, current_pc);
        cpu->running = false;
        break;
    }
}

// Load program into memory
bool rv32_cpu_load_program(rv32_cpu_t *cpu, const uint8_t *program, size_t size, uint32_t base_addr) {
    if (base_addr < RAM_BASE || base_addr + size > RAM_BASE + RAM_SIZE) {
        fprintf(stderr, "Program too large or invalid base address\n");
        return false;
    }

    uint32_t offset = base_addr - RAM_BASE;
    memcpy(&cpu->memory[offset], program, size);
    return true;
}

// Print CPU state for debugging
void rv32_cpu_print_state(rv32_cpu_t *cpu) {
    printf("PC: 0x%08x  Instructions: %llu\n", cpu->pc, cpu->instruction_count);

    for (int i = 0; i < 32; i += 4) {
        printf("x%2d: 0x%08x  x%2d: 0x%08x  x%2d: 0x%08x  x%2d: 0x%08x\n",
               i,     cpu->regs[i],
               i + 1, cpu->regs[i + 1],
               i + 2, cpu->regs[i + 2],
               i + 3, cpu->regs[i + 3]);
    }
    printf("\n");
}

// Simple disassembler for debugging
void rv32_cpu_disassemble(uint32_t instruction, uint32_t pc, char *buffer, size_t buf_size) {
    uint32_t opcode = instruction & 0x7F;
    uint32_t rd = (instruction >> 7) & 0x1F;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;
    uint32_t funct3 = (instruction >> 12) & 0x7;

    switch (opcode) {
    case OPCODE_LUI:
        snprintf(buffer, buf_size, "lui x%d, 0x%05x", rd, instruction >> 12);
        break;
    case OPCODE_AUIPC:
        snprintf(buffer, buf_size, "auipc x%d, 0x%05x", rd, instruction >> 12);
        break;
    case OPCODE_JAL:
        snprintf(buffer, buf_size, "jal x%d, 0x%08x", rd, pc + sign_extend(
            ((instruction >> 31) << 20) |
            (((instruction >> 12) & 0xFF) << 12) |
            (((instruction >> 20) & 0x1) << 11) |
            (((instruction >> 21) & 0x3FF) << 1), 21));
        break;
    case OPCODE_OP_IMM:
        switch (funct3) {
        case 0x0: snprintf(buffer, buf_size, "addi x%d, x%d, %d", rd, rs1, sign_extend((instruction >> 20) & 0xFFF, 12)); break;
        case 0x4: snprintf(buffer, buf_size, "xori x%d, x%d, %d", rd, rs1, sign_extend((instruction >> 20) & 0xFFF, 12)); break;
        case 0x6: snprintf(buffer, buf_size, "ori x%d, x%d, %d", rd, rs1, sign_extend((instruction >> 20) & 0xFFF, 12)); break;
        case 0x7: snprintf(buffer, buf_size, "andi x%d, x%d, %d", rd, rs1, sign_extend((instruction >> 20) & 0xFFF, 12)); break;
        default: snprintf(buffer, buf_size, "unknown_op_imm 0x%08x", instruction); break;
        }
        break;
    case OPCODE_OP:
        switch (funct3) {
        case 0x0:
            if ((instruction >> 30) & 1) {
                snprintf(buffer, buf_size, "sub x%d, x%d, x%d", rd, rs1, rs2);
            } else {
                snprintf(buffer, buf_size, "add x%d, x%d, x%d", rd, rs1, rs2);
            }
            break;
        case 0x4: snprintf(buffer, buf_size, "xor x%d, x%d, x%d", rd, rs1, rs2); break;
        case 0x6: snprintf(buffer, buf_size, "or x%d, x%d, x%d", rd, rs1, rs2); break;
        case 0x7: snprintf(buffer, buf_size, "and x%d, x%d, x%d", rd, rs1, rs2); break;
        default: snprintf(buffer, buf_size, "unknown_op 0x%08x", instruction); break;
        }
        break;
    case OPCODE_SYSTEM:
        if (((instruction >> 20) & 0xFFF) == 0x0) {
            snprintf(buffer, buf_size, "ecall");
        } else if (((instruction >> 20) & 0xFFF) == 0x1) {
            snprintf(buffer, buf_size, "ebreak");
        } else {
            snprintf(buffer, buf_size, "unknown_system 0x%08x", instruction);
        }
        break;
    default:
        snprintf(buffer, buf_size, "unknown 0x%08x", instruction);
        break;
    }
}