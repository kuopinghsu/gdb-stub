#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <fcntl.h>

#include "../src/gdb_stub.h"
#include "rv32_cpu.h"

// Global variables for signal handling
static rv32_cpu_t *g_cpu = NULL;
gdb_context_t *g_gdb_ctx = NULL;  // Made global for CPU watchpoint access
static volatile bool g_interrupt_received = false;

// Signal handler for Ctrl+C
void sigint_handler(int sig) {
    (void)sig;
    g_interrupt_received = true;
    if (g_gdb_ctx && g_gdb_ctx->stub.connected) {
        g_gdb_ctx->should_stop = true;
    }
}

// GDB callback functions
uint32_t gdb_read_reg(void *sim, int reg_num) {
    rv32_cpu_t *cpu = (rv32_cpu_t *)sim;

    if (reg_num >= 0 && reg_num < 32) {
        return rv32_cpu_read_reg(cpu, reg_num);
    } else if (reg_num == 32) {  // PC register
        return cpu->pc;
    }
    return 0;
}

void gdb_write_reg(void *sim, int reg_num, uint32_t value) {
    rv32_cpu_t *cpu = (rv32_cpu_t *)sim;

    if (reg_num >= 0 && reg_num < 32) {
        rv32_cpu_write_reg(cpu, reg_num, value);
    } else if (reg_num == 32) {  // PC register
        cpu->pc = value;
    }
}

uint32_t gdb_read_mem(void *sim, uint32_t addr, int size) {
    rv32_cpu_t *cpu = (rv32_cpu_t *)sim;
    return rv32_cpu_read_mem(cpu, addr, size);
}

void gdb_write_mem(void *sim, uint32_t addr, uint32_t value, int size) {
    rv32_cpu_t *cpu = (rv32_cpu_t *)sim;
    rv32_cpu_write_mem(cpu, addr, value, size);
}

uint32_t gdb_get_pc(void *sim) {
    rv32_cpu_t *cpu = (rv32_cpu_t *)sim;
    return cpu->pc;
}

void gdb_set_pc(void *sim, uint32_t pc) {
    rv32_cpu_t *cpu = (rv32_cpu_t *)sim;
    cpu->pc = pc;
}

void gdb_single_step(void *sim) {
    rv32_cpu_t *cpu = (rv32_cpu_t *)sim;
    rv32_cpu_step(cpu);
}

bool gdb_is_running(void *sim) {
    rv32_cpu_t *cpu = (rv32_cpu_t *)sim;
    return cpu->running;
}

void gdb_reset(void *sim) {
    rv32_cpu_t *cpu = (rv32_cpu_t *)sim;
    rv32_cpu_reset(cpu);
}

void gdb_resume(void *sim) {
    rv32_cpu_t *cpu = (rv32_cpu_t *)sim;
    cpu->halted = false;  // Clear halted flag to resume execution
}

// Create a simple test program
void create_test_program(rv32_cpu_t *cpu) {
    // RISC-V program with memory operations for watchpoint demo
    uint32_t program[] = {
        // _start:
        0x800002b7,  // lui  t0, 0x80000       # Load base address
        0x00000313,  // addi t1, zero, 0       # Counter = 0
        0x00500393,  // addi t2, zero, 5       # Limit = 5 (reduced for demo)
        0x80001437,  // lui  s0, 0x80001       # Memory area for watchpoint demo
        0x00040413,  // addi s0, s0, 0         # s0 = 0x80001000 (watchpoint target)

        // loop: (address 0x80000014)
        0x00642023,  // sw   t1, 0(s0)         # Store counter to 0x80001000 (write watchpoint)
        0x00042e03,  // lw   t3, 0(s0)         # Load from 0x80001000 (read watchpoint)
        0x00130313,  // addi t1, t1, 1         # counter++
        0x00731463,  // beq  t1, t2, end       # if counter == limit, go to end (offset +8)
        0xff1ff06f,  // jal  loop              # jump back to loop

        // end: (address 0x80000028)
        0x00642223,  // sw   t1, 4(s0)         # Store final value to 0x80001004 (access watchpoint)
        0xffdff06f,  // jal  end               # Infinite loop at end (no ecall)
    };

    // Load program into memory
    rv32_cpu_load_program(cpu, (uint8_t *)program, sizeof(program), RAM_BASE);
}

void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -p PORT     GDB server port (default: 1234)\n");
    printf("  -d          Enable debug output\n");
    printf("  -i          Interactive mode (step through instructions)\n");
    printf("  -h          Show this help\n");
    printf("\n");
    printf("To connect with GDB:\n");
    printf("  gdb\n");
    printf("  (gdb) target remote localhost:1234\n");
    printf("  (gdb) continue\n");
}

int main(int argc, char *argv[]) {
    uint16_t gdb_port = 1234;
    bool debug_mode = false;
    bool interactive_mode = false;

    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "p:dih")) != -1) {
        switch (opt) {
        case 'p':
            gdb_port = (uint16_t)atoi(optarg);
            if (gdb_port == 0) {
                fprintf(stderr, "Invalid port number: %s\n", optarg);
                return 1;
            }
            break;
        case 'd':
            debug_mode = true;
            break;
        case 'i':
            interactive_mode = true;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    printf("RISC-V RV32I Emulator with GDB Stub\n");
    printf("===================================\n");

    // Initialize CPU
    rv32_cpu_t cpu;
    rv32_cpu_init(&cpu);
    g_cpu = &cpu;

    // Load test program
    create_test_program(&cpu);
    printf("Test program loaded at 0x%08x\n", RAM_BASE);

    // Initialize GDB stub
    gdb_context_t gdb_ctx;
    if (gdb_stub_init(&gdb_ctx, gdb_port) < 0) {
        fprintf(stderr, "Failed to initialize GDB stub\n");
        return 1;
    }
    g_gdb_ctx = &gdb_ctx;

    // Set up GDB callbacks
    gdb_callbacks_t callbacks = {
        .read_reg = gdb_read_reg,
        .write_reg = gdb_write_reg,
        .read_mem = gdb_read_mem,
        .write_mem = gdb_write_mem,
        .get_pc = gdb_get_pc,
        .set_pc = gdb_set_pc,
        .single_step = gdb_single_step,
        .is_running = gdb_is_running,
        .reset = gdb_reset,
        .resume = gdb_resume,
    };

    // Set up signal handling
    signal(SIGINT, sigint_handler);

    // Wait for GDB connection
    if (gdb_stub_accept(&gdb_ctx) < 0) {
        fprintf(stderr, "Failed to accept GDB connection\n");
        gdb_stub_close(&gdb_ctx);
        return 1;
    }

    printf("\nGDB connected! CPU execution will be controlled by GDB.\n");
    printf("In GDB, use these commands:\n");
    printf("  info registers  - Show CPU registers\n");
    printf("  x/10i $pc      - Disassemble 10 instructions at PC\n");
    printf("  break *0x80000000 - Set breakpoint at address\n");
    printf("  step           - Single step\n");
    printf("  continue       - Continue execution\n");
    printf("  quit           - Exit GDB\n\n");

    // Make stdin non-blocking for interactive mode
    if (interactive_mode) {
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    // Main execution loop
    bool running = true;
    bool cpu_was_running = false;

    while (running && gdb_ctx.stub.connected) {
        // Process GDB commands
        int gdb_result = gdb_stub_process(&gdb_ctx, &cpu, &callbacks);

        if (gdb_result < 0) {
            // GDB disconnected or error
            break;
        } else if (gdb_result == 1) {
            // GDB wants to continue/step
            cpu_was_running = true;

            if (gdb_ctx.single_step) {
                // Single step mode
                if (debug_mode) {
                    char disasm[128];
                    uint32_t instruction = rv32_cpu_read_mem(&cpu, cpu.pc, 4);
                    rv32_cpu_disassemble(instruction, cpu.pc, disasm, sizeof(disasm));
                    printf("Executing: 0x%08x: %s\n", cpu.pc, disasm);
                }

                // Check for breakpoint before execution
                if (gdb_stub_check_breakpoint(&gdb_ctx, cpu.pc)) {
                    gdb_ctx.should_stop = true;
                    gdb_stub_send_stop_reason(&gdb_ctx, 5, cpu.pc);  // SIGTRAP
                    continue;
                }

                rv32_cpu_step(&cpu);

                if (!cpu.running || cpu.halted) {
                    if (cpu.halted) {
                        // Watchpoint or breakpoint hit
                        gdb_stub_send_stop_reason(&gdb_ctx, 5, cpu.pc);  // SIGTRAP
                    } else {
                        gdb_stub_send_stop_signal(&gdb_ctx, 9);  // SIGKILL
                        printf("CPU halted\n");
                        break;
                    }
                }

                // Single step completed
                gdb_ctx.single_step = false;
                gdb_ctx.should_stop = true;
                gdb_stub_send_stop_reason(&gdb_ctx, 5, cpu.pc);  // SIGTRAP

            } else {
                // Continue mode - run until breakpoint or interrupt
                while (cpu.running && !cpu.halted && !gdb_ctx.should_stop && !g_interrupt_received) {
                    // Check for breakpoint
                    if (gdb_stub_check_breakpoint(&gdb_ctx, cpu.pc)) {
                        gdb_ctx.should_stop = true;
                        gdb_stub_send_stop_reason(&gdb_ctx, 5, cpu.pc);  // SIGTRAP
                        break;
                    }

                    if (debug_mode && (cpu.instruction_count % 1000 == 0)) {
                        printf("PC: 0x%08x, Instructions: %lu\n", cpu.pc, cpu.instruction_count);
                    }

                    rv32_cpu_step(&cpu);

                    // If watchpoint was hit, stop execution
                    if (cpu.halted) {
                        gdb_ctx.should_stop = true;
                        gdb_stub_send_stop_reason(&gdb_ctx, 5, cpu.pc);  // SIGTRAP
                        break;
                    }

                    // Check for GDB commands (non-blocking)
                    fd_set readfds;
                    struct timeval timeout = {0, 0}; // Non-blocking

                    FD_ZERO(&readfds);
                    FD_SET(gdb_ctx.stub.client_fd, &readfds);

                    int select_result = select(gdb_ctx.stub.client_fd + 1, &readfds, NULL, NULL, &timeout);
                    if (select_result > 0 && FD_ISSET(gdb_ctx.stub.client_fd, &readfds)) {
                        // GDB has sent a command (likely Ctrl-C)
                        break;
                    }
                }

                if (!cpu.running) {
                    gdb_stub_send_stop_signal(&gdb_ctx, 9);  // SIGKILL
                    printf("CPU halted\n");
                } else if (g_interrupt_received) {
                    gdb_stub_send_stop_signal(&gdb_ctx, 2);  // SIGINT
                    g_interrupt_received = false;
                    printf("Execution interrupted\n");
                }
            }
        }

        if (interactive_mode && cpu_was_running && gdb_ctx.should_stop) {
            printf("\nCPU State:\n");
            printf("PC: 0x%08x  Instructions executed: %lu\n", cpu.pc, cpu.instruction_count);
            printf("Press Enter to continue, 'q' to quit, 'r' to show registers: ");
            fflush(stdout);

            char input[10];
            if (fgets(input, sizeof(input), stdin)) {
                if (input[0] == 'q') {
                    running = false;
                } else if (input[0] == 'r') {
                    rv32_cpu_print_state(&cpu);
                }
            }
        }
    }

    // Cleanup
    printf("\nShutting down...\n");
    gdb_stub_close(&gdb_ctx);
    return 0;
}
