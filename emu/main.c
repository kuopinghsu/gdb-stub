#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "../src/gdb_stub.h"
#include "rv32_cpu.h"
#include "rv32_system.h"
#include "smp_sync.h"
#include "smp_program.h"

static bool valid_hart_count(int n) {
    return n == 1 || n == 2 || n == 4 || n == 8;
}

static rv32_system_t g_sys;
static gdb_context_t *g_gdb_ctx = NULL;
static volatile bool g_interrupt_received = false;

static rv32_cpu_t *sys_focus_cpu(void) {
    return &g_sys.harts[g_sys.focus_hart];
}

static rv32_cpu_t *sys_hart_cpu(int hart) {
    return &g_sys.harts[hart];
}

static int sys_thread_id(int hart) {
    return hart + 1;
}

static bool sys_any_running(void) {
    for (int h = 0; h < g_sys.num_harts; h++) {
        if (g_sys.harts[h].running && !g_sys.harts[h].halted) {
            return true;
        }
    }
    return false;
}

static void sys_halt_all(void) {
    for (int h = 0; h < g_sys.num_harts; h++) {
        g_sys.harts[h].halted = true;
    }
}

static void sys_resume_runnable(void) {
    for (int h = 0; h < g_sys.num_harts; h++) {
        if (g_gdb_ctx && !gdb_stub_should_run_thread(g_gdb_ctx, sys_thread_id(h))) {
            continue;
        }
        g_sys.harts[h].halted = false;
    }
}

static uint32_t sys_read_mem(rv32_system_t *sys, uint32_t addr, int size) {
    if (addr >= SMP_SYNC_BASE && addr < SMP_SYNC_BASE + 0x40) {
        return smp_sync_read_ram(sys, addr, size);
    }
    if (addr == SMP_IPI_MMIO) {
        return smp_sync_read_mmio(sys, addr, size);
    }
    return rv32_cpu_read_mem(&sys->harts[0], addr, size);
}

static void sys_write_mem(rv32_system_t *sys, rv32_cpu_t *src, uint32_t addr,
                          uint32_t value, int size) {
    if (smp_sync_handle_mmio(&sys->sync, sys, addr, value, size)) {
        return;
    }
    if (addr >= SMP_SYNC_BASE && addr < SMP_SYNC_BASE + 0x40) {
        smp_sync_write_ram(sys, addr, value, size);
        return;
    }

    rv32_cpu_write_mem(src ? src : &sys->harts[0], addr, value, size);
}

static void sys_post_step(rv32_system_t *sys, rv32_cpu_t *cpu, uint32_t prev_pc) {
    if (sys->num_harts <= 1) {
        return;
    }
    if (cpu->hart_id == 0 && prev_pc == 0x80000014) {
        smp_sync_on_counter_store(&sys->sync, sys, 0, cpu->regs[RV32_REG_T1]);
    }
}

static void sys_cpu_step(rv32_system_t *sys, rv32_cpu_t *cpu) {
    uint32_t prev_pc = cpu->pc;
    rv32_cpu_step(cpu);
    sys_post_step(sys, cpu, prev_pc);
}

static void notify_stop(gdb_context_t *ctx, int signal, uint32_t pc) {
    int tid = g_sys.stop_hart >= 0 ? sys_thread_id(g_sys.stop_hart) : sys_thread_id(g_sys.focus_hart);
    gdb_stub_set_stop_thread(ctx, tid);
    gdb_stub_send_stop_reason(ctx, signal, pc);
}

static bool handle_hart_stop(gdb_context_t *ctx, rv32_cpu_t *cpu, int signal) {
    g_sys.stop_hart = cpu->hart_id;
    g_sys.focus_hart = cpu->hart_id;
    ctx->should_stop = true;
    notify_stop(ctx, signal, cpu->pc);
    return true;
}

void sigint_handler(int sig) {
    (void)sig;
    g_interrupt_received = true;
    if (g_gdb_ctx && g_gdb_ctx->stub.connected) {
        g_gdb_ctx->should_stop = true;
        sys_halt_all();
        g_sys.stop_hart = g_sys.focus_hart;
    }
}

uint32_t gdb_read_reg(void *sim, int reg_num) {
    rv32_system_t *sys = (rv32_system_t *)sim;
    rv32_cpu_t *cpu = &sys->harts[sys->focus_hart];

    if (reg_num >= 0 && reg_num < 32) {
        return rv32_cpu_read_reg(cpu, reg_num);
    } else if (reg_num == 32) {
        return cpu->pc;
    }
    return 0;
}

void gdb_write_reg(void *sim, int reg_num, uint32_t value) {
    rv32_system_t *sys = (rv32_system_t *)sim;
    rv32_cpu_t *cpu = &sys->harts[sys->focus_hart];

    if (reg_num >= 0 && reg_num < 32) {
        rv32_cpu_write_reg(cpu, reg_num, value);
    } else if (reg_num == 32) {
        cpu->pc = value;
    }
}

uint32_t gdb_read_mem(void *sim, uint32_t addr, int size) {
    return sys_read_mem((rv32_system_t *)sim, addr, size);
}

void gdb_write_mem(void *sim, uint32_t addr, uint32_t value, int size) {
    rv32_system_t *sys = (rv32_system_t *)sim;
    sys_write_mem(sys, &sys->harts[sys->focus_hart], addr, value, size);
}

uint32_t gdb_get_pc(void *sim) {
    rv32_system_t *sys = (rv32_system_t *)sim;
    return sys->harts[sys->focus_hart].pc;
}

void gdb_set_pc(void *sim, uint32_t pc) {
    rv32_system_t *sys = (rv32_system_t *)sim;
    sys->harts[sys->focus_hart].pc = pc;
}

void gdb_single_step(void *sim) {
    rv32_system_t *sys = (rv32_system_t *)sim;
    sys_cpu_step(sys, &sys->harts[sys->focus_hart]);
}

bool gdb_is_running(void *sim) {
    (void)sim;
    return sys_any_running();
}

void gdb_reset(void *sim) {
    rv32_system_t *sys = (rv32_system_t *)sim;

    smp_sync_reset(&sys->sync, sys->num_harts);
    for (int h = 0; h < sys->num_harts; h++) {
        rv32_cpu_reset(&sys->harts[h]);
        sys->harts[h].regs[RV32_REG_TP] = (uint32_t)h;
        if (h > 0) {
            smp_sync_init_worker_hart(sys, &sys->harts[h]);
        }
    }
    smp_sync_ram_init(sys);
    sys->focus_hart = 0;
    sys->stop_hart = -1;
}

void gdb_resume(void *sim) {
    (void)sim;
    sys_resume_runnable();
}

int gdb_get_num_harts(void *sim) {
    return ((rv32_system_t *)sim)->num_harts;
}

void gdb_set_focus_hart(void *sim, int hart) {
    rv32_system_t *sys = (rv32_system_t *)sim;
    if (hart >= 0 && hart < sys->num_harts) {
        sys->focus_hart = hart;
    }
}

int gdb_get_focus_hart(void *sim) {
    return ((rv32_system_t *)sim)->focus_hart;
}

int gdb_get_stop_hart(void *sim) {
    return ((rv32_system_t *)sim)->stop_hart;
}

void gdb_halt_cpus(void *sim) {
    (void)sim;
    sys_halt_all();
    g_sys.stop_hart = g_sys.focus_hart;
}

static bool gdb_watchpoint_read(void *ctx, uint32_t addr, uint32_t len) {
    rv32_cpu_t *cpu = (rv32_cpu_t *)ctx;
    gdb_context_t *gdb = g_gdb_ctx;
    if (!gdb || !gdb->stub.connected) {
        return false;
    }
    if (gdb_stub_check_watchpoint_read(gdb, addr, len)) {
        g_sys.stop_hart = cpu->hart_id;
        return true;
    }
    return false;
}

static bool gdb_watchpoint_write(void *ctx, uint32_t addr, uint32_t len) {
    rv32_cpu_t *cpu = (rv32_cpu_t *)ctx;
    gdb_context_t *gdb = g_gdb_ctx;
    if (!gdb || !gdb->stub.connected) {
        return false;
    }
    if (gdb_stub_check_watchpoint_write(gdb, addr, len)) {
        g_sys.stop_hart = cpu->hart_id;
        return true;
    }
    return false;
}

static void setup_hart_watchpoints(rv32_cpu_t *cpu) {
    cpu->watchpoint_ctx = cpu;
    cpu->check_watchpoint_read = gdb_watchpoint_read;
    cpu->check_watchpoint_write = gdb_watchpoint_write;
}

void create_test_program(rv32_system_t *sys) {
    rv32_cpu_t *cpu0 = &sys->harts[0];
    uint32_t program[] = {
        // _start:
        0x800002b7,  // lui  t0, 0x80000
        0x00000313,  // addi t1, zero, 0
        0x00500393,  // addi t2, zero, 5
        0x80001437,  // lui  s0, 0x80001       # s0 = 0x80001000
        0x00040413,  // addi s0, s0, 0

        // loop: (0x80000014)
        0x00642023,  // sw   t1, 0(s0)         # triggers worker IPI dispatch
        0x00042e03,  // lw   t3, 0(s0)
        0x00130313,  // addi t1, t1, 1
        0x00731463,  // beq  t1, t2, end
        0xff1ff06f,  // jal  loop

        // end: (0x80000028)
        0x00642223,  // sw   t1, 4(s0)
        0x0000006f,  // jal  end              # spin at 0x8000002c
    };

    rv32_cpu_load_program(cpu0, (uint8_t *)program, sizeof(program), RAM_BASE);
    memcpy(cpu0->memory + (SMP_WORKER_WAIT - RAM_BASE),
           smp_worker_code, sizeof(smp_worker_code));

    smp_sync_reset(&sys->sync, sys->num_harts);
    smp_sync_ram_init(sys);

    for (int h = 1; h < sys->num_harts; h++) {
        smp_sync_init_worker_hart(sys, &sys->harts[h]);
    }
}

static void init_system(rv32_system_t *sys, int num_harts) {
    if (!valid_hart_count(num_harts)) {
        num_harts = 1;
    }

    memset(sys, 0, sizeof(*sys));
    sys->num_harts = num_harts;
    sys->focus_hart = 0;
    sys->stop_hart = -1;

    for (int h = 0; h < num_harts; h++) {
        rv32_cpu_init(&sys->harts[h]);
        sys->harts[h].hart_id = h;
        sys->harts[h].regs[RV32_REG_TP] = (uint32_t)h;
        if (h > 0) {
            sys->harts[h].shared_ram = sys->harts[0].memory;
        }
        setup_hart_watchpoints(&sys->harts[h]);
    }

    create_test_program(sys);
}

static bool should_step_hart(int h, gdb_context_t *ctx, bool single_step_mode) {
    if (!gdb_stub_should_run_thread(ctx, sys_thread_id(h))) {
        return false;
    }
    if (g_sys.num_harts == 1) {
        return true;
    }
    if (single_step_mode) {
        return h == g_sys.focus_hart;
    }
    if (ctx->continue_thread < 0) {
        if (h == 0) {
            return true;
        }
        return smp_sync_worker_should_run(&g_sys, h);
    }
    return ctx->continue_thread == sys_thread_id(h);
}

static bool step_focus_hart(gdb_context_t *ctx, bool debug_mode) {
    rv32_cpu_t *cpu = sys_focus_cpu();

    if (!gdb_stub_should_run_thread(ctx, sys_thread_id(cpu->hart_id))) {
        gdb_stub_set_stop_thread(ctx, sys_thread_id(cpu->hart_id));
        gdb_stub_send_stop_reason(ctx, 0, cpu->pc);
        return true;
    }

    if (debug_mode) {
        char disasm[128];
        uint32_t instruction = rv32_cpu_read_mem(cpu, cpu->pc, 4);
        rv32_cpu_disassemble(instruction, cpu->pc, disasm, sizeof(disasm));
        printf("Hart %d executing: 0x%08x: %s\n", cpu->hart_id, cpu->pc, disasm);
    }

    if (gdb_stub_check_breakpoint(ctx, cpu->pc)) {
        ctx->should_stop = true;
        return handle_hart_stop(ctx, cpu, 5);
    }

    sys_cpu_step(&g_sys, cpu);

    if (cpu->halted || !cpu->running) {
        ctx->should_stop = true;
        ctx->single_step = false;
        if (cpu->halted) {
            return handle_hart_stop(ctx, cpu, 5);
        }
        return handle_hart_stop(ctx, cpu, 4);
    }

    ctx->single_step = false;
    ctx->should_stop = true;
    return handle_hart_stop(ctx, cpu, 5);
}

static bool continue_all_harts(gdb_context_t *ctx, bool debug_mode) {
    bool stop_sent = false;

    while (sys_any_running() && !ctx->should_stop && !g_interrupt_received) {
        for (int h = 0; h < g_sys.num_harts; h++) {
            rv32_cpu_t *cpu = sys_hart_cpu(h);

            if (!should_step_hart(h, ctx, false)) {
                continue;
            }
            if (!cpu->running || cpu->halted) {
                continue;
            }

            if (gdb_stub_check_breakpoint(ctx, cpu->pc)) {
                stop_sent = handle_hart_stop(ctx, cpu, 5);
                break;
            }

            if (debug_mode && (cpu->instruction_count % 1000 == 0)) {
                printf("Hart %d PC: 0x%08x, Instructions: %llu\n",
                       h, cpu->pc, cpu->instruction_count);
            }

            sys_cpu_step(&g_sys, cpu);

            if (cpu->halted) {
                stop_sent = handle_hart_stop(ctx, cpu, 5);
                break;
            }

            if (!cpu->running) {
                stop_sent = handle_hart_stop(ctx, cpu, 4);
                break;
            }
        }

        if (stop_sent) {
            break;
        }

        fd_set readfds;
        struct timeval timeout = {0, 0};

        FD_ZERO(&readfds);
        FD_SET(ctx->stub.client_fd, &readfds);

        int select_result = select(ctx->stub.client_fd + 1, &readfds, NULL, NULL, &timeout);
        if (select_result > 0 && FD_ISSET(ctx->stub.client_fd, &readfds)) {
            char peek;
            if (recv(ctx->stub.client_fd, &peek, 1, MSG_PEEK | MSG_DONTWAIT) > 0) {
                ctx->should_stop = true;
                break;
            }
        }
    }

    if (!stop_sent && !sys_any_running()) {
        ctx->should_stop = true;
        rv32_cpu_t *cpu = sys_focus_cpu();
        notify_stop(ctx, 4, cpu->pc);
        printf("CPU halted\n");
        stop_sent = true;
    } else if (!stop_sent && g_interrupt_received) {
        g_interrupt_received = false;
        ctx->should_stop = true;
        rv32_cpu_t *cpu = sys_focus_cpu();
        notify_stop(ctx, 2, cpu->pc);
        printf("Execution interrupted\n");
        stop_sent = true;
    } else if (!stop_sent && ctx->should_stop) {
        rv32_cpu_t *cpu = sys_focus_cpu();
        notify_stop(ctx, 2, cpu->pc);
        stop_sent = true;
    }

    return stop_sent;
}

void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -p PORT     GDB server port (default: 1234)\n");
    printf("  -c HARTS    Number of harts: 1, 2, 4, or 8 (default: 1)\n");
    printf("  -d          Enable debug output\n");
    printf("  -i          Interactive mode (step through instructions)\n");
    printf("  -h          Show this help\n");
    printf("\n");
    printf("To connect with GDB:\n");
    printf("  riscv-none-elf-gdb\n");
    printf("  (gdb) target remote localhost:1234\n");
    printf("  (gdb) continue\n");
}

int main(int argc, char *argv[]) {
    uint16_t gdb_port = 1234;
    int num_harts = 1;
    bool debug_mode = false;
    bool interactive_mode = false;

    int opt;
    while ((opt = getopt(argc, argv, "p:c:dih")) != -1) {
        switch (opt) {
        case 'p':
            gdb_port = (uint16_t)atoi(optarg);
            if (gdb_port == 0) {
                fprintf(stderr, "Invalid port number: %s\n", optarg);
                return 1;
            }
            break;
        case 'c':
            num_harts = atoi(optarg);
            if (!valid_hart_count(num_harts)) {
                fprintf(stderr, "Invalid hart count: %s (use 1, 2, 4, or 8)\n", optarg);
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

    init_system(&g_sys, num_harts);
    printf("Test program loaded at 0x%08x (%d hart%s)\n",
           RAM_BASE, g_sys.num_harts, g_sys.num_harts == 1 ? "" : "s");
    if (g_sys.num_harts > 1) {
        printf("Hart 0 runs demo; harts 1-%d are worker harts at 0x%08x (RV32I poll/handler)\n",
               g_sys.num_harts - 1, SMP_WORKER_POLL);
        printf("Shared sync at 0x%08x, IPI MMIO at 0x%08x\n",
               SMP_SYNC_BASE, SMP_IPI_MMIO);
    }

    gdb_context_t gdb_ctx;
    if (gdb_stub_init(&gdb_ctx, gdb_port) < 0) {
        fprintf(stderr, "Failed to initialize GDB stub\n");
        return 1;
    }
    g_gdb_ctx = &gdb_ctx;

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
        .get_num_harts = gdb_get_num_harts,
        .set_focus_hart = gdb_set_focus_hart,
        .get_focus_hart = gdb_get_focus_hart,
        .get_stop_hart = gdb_get_stop_hart,
        .halt_cpus = gdb_halt_cpus,
    };

    signal(SIGINT, sigint_handler);

    if (gdb_stub_accept(&gdb_ctx) < 0) {
        fprintf(stderr, "Failed to accept GDB connection\n");
        gdb_stub_close(&gdb_ctx);
        return 1;
    }

    printf("\nGDB connected! CPU execution will be controlled by GDB.\n");
    if (g_sys.num_harts > 1) {
        printf("SMP mode: use 'info threads' and 'thread N' to switch harts.\n");
    }
    printf("In GDB, use these commands:\n");
    printf("  info registers  - Show CPU registers\n");
    printf("  x/10i $pc      - Disassemble 10 instructions at PC\n");
    printf("  break *0x80000000 - Set breakpoint at address\n");
    printf("  step           - Single step\n");
    printf("  continue       - Continue execution\n");
    printf("  quit           - Exit GDB\n\n");

    if (interactive_mode) {
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    bool running = true;
    bool cpu_was_running = false;

    while (running && gdb_ctx.stub.connected) {
        int gdb_result = gdb_stub_process(&gdb_ctx, &g_sys, &callbacks);

        if (gdb_result < 0) {
            break;
        } else if (gdb_result == 1) {
            cpu_was_running = true;
            sys_resume_runnable();

            if (gdb_ctx.single_step) {
                step_focus_hart(&gdb_ctx, debug_mode);
            } else {
                continue_all_harts(&gdb_ctx, debug_mode);
            }
        }

        if (interactive_mode && cpu_was_running && gdb_ctx.should_stop) {
            rv32_cpu_t *cpu = sys_focus_cpu();
            printf("\nCPU State (hart %d):\n", cpu->hart_id);
            printf("PC: 0x%08x  Instructions executed: %llu\n", cpu->pc, cpu->instruction_count);
            printf("Press Enter to continue, 'q' to quit, 'r' to show registers: ");
            fflush(stdout);

            char input[10];
            if (fgets(input, sizeof(input), stdin)) {
                if (input[0] == 'q') {
                    running = false;
                } else if (input[0] == 'r') {
                    rv32_cpu_print_state(cpu);
                }
            }
        }
    }

    printf("\nShutting down...\n");
    gdb_stub_close(&gdb_ctx);
    return 0;
}
