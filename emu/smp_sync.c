#include "smp_sync.h"
#include "smp_program.h"
#include "rv32_system.h"
#include <string.h>

static uint8_t *sync_ram(rv32_system_t *sys) {
    return sys->harts[0].shared_ram ? sys->harts[0].shared_ram : sys->harts[0].memory;
}

static uint32_t ram_read32(rv32_system_t *sys, uint32_t addr) {
    uint8_t *ram = sync_ram(sys);
    if (!ram || addr < RAM_BASE || addr + 4 > RAM_BASE + RAM_SIZE) {
        return 0;
    }
    return *(uint32_t *)(ram + (addr - RAM_BASE));
}

static void ram_write32(rv32_system_t *sys, uint32_t addr, uint32_t value) {
    uint8_t *ram = sync_ram(sys);
    if (!ram || addr < RAM_BASE || addr + 4 > RAM_BASE + RAM_SIZE) {
        return;
    }
    *(uint32_t *)(ram + (addr - RAM_BASE)) = value;
}

static int smp_worker_target(rv32_system_t *sys, uint32_t work_item) {
    int workers = sys->num_harts - 1;
    if (workers <= 0) {
        return -1;
    }
    return (int)(work_item % (uint32_t)workers) + 1;
}

static void smp_deliver_ipi(smp_sync_t *sync, rv32_system_t *sys,
                            int target_hart, uint32_t work_item) {
    if (target_hart <= 0 || target_hart >= sys->num_harts) {
        return;
    }

    ram_write32(sys, SMP_SYNC_BASE + SMP_OFF_WORK_ITEM, work_item);
    ram_write32(sys, SMP_SYNC_BASE + SMP_OFF_WORK_READY, 1);
    ram_write32(sys, SMP_SYNC_BASE + SMP_OFF_IPI_PENDING + (uint32_t)target_hart * 4, 1);
    (void)sync;
}

void smp_sync_init(smp_sync_t *sync, int num_harts) {
    smp_sync_reset(sync, num_harts);
}

void smp_sync_reset(smp_sync_t *sync, int num_harts) {
    memset(sync, 0, sizeof(*sync));
    sync->num_harts = (uint32_t)num_harts;
}

void smp_sync_ram_init(rv32_system_t *sys) {
    ram_write32(sys, SMP_SYNC_BASE + SMP_OFF_LOCK, 0);
    ram_write32(sys, SMP_SYNC_BASE + SMP_OFF_WORK_ITEM, 0);
    ram_write32(sys, SMP_SYNC_BASE + SMP_OFF_WORK_READY, 0);
    ram_write32(sys, SMP_SYNC_BASE + SMP_OFF_SHARED_DONE, 0);
    ram_write32(sys, SMP_SYNC_BASE + SMP_OFF_NUM_HARTS, (uint32_t)sys->num_harts);
    for (int h = 0; h < RV32_MAX_HARTS; h++) {
        ram_write32(sys, SMP_SYNC_BASE + SMP_OFF_IPI_PENDING + (uint32_t)h * 4, 0);
    }
}

void smp_sync_init_worker_hart(rv32_system_t *sys, rv32_cpu_t *cpu) {
    (void)sys;
    cpu->regs[RV32_REG_S0] = SMP_SYNC_BASE;
    cpu->pc = SMP_WORKER_POLL;
}

bool smp_sync_worker_should_run(rv32_system_t *sys, int hart_id) {
    if (hart_id <= 0 || hart_id >= sys->num_harts) {
        return false;
    }

    rv32_cpu_t *cpu = &sys->harts[hart_id];
    if (cpu->pc >= SMP_WORKER_HANDLE && cpu->pc < SMP_WORKER_END) {
        return true;
    }
    if (ram_read32(sys, SMP_SYNC_BASE + SMP_OFF_IPI_PENDING + (uint32_t)hart_id * 4) != 0) {
        return true;
    }
    return false;
}

void smp_sync_run_worker_jobs(rv32_system_t *sys, int hart_id) {
    if (hart_id <= 0 || hart_id >= sys->num_harts) {
        return;
    }

    rv32_cpu_t *cpu = &sys->harts[hart_id];
    if (!smp_sync_worker_should_run(sys, hart_id)) {
        return;
    }

    unsigned guard = 128;
    while (guard-- > 0 && cpu->running && !cpu->halted) {
        rv32_cpu_step(cpu);

        if (cpu->pc == SMP_WORKER_POLL || cpu->pc == SMP_WORKER_SPIN) {
            if (ram_read32(sys, SMP_SYNC_BASE + SMP_OFF_IPI_PENDING +
                           (uint32_t)hart_id * 4) == 0) {
                break;
            }
        }
    }
}

bool smp_sync_handle_mmio(smp_sync_t *sync, rv32_system_t *sys,
                          uint32_t addr, uint32_t value, int size) {
    if (addr != SMP_IPI_MMIO || size != 4) {
        return false;
    }

    int target = (int)((value >> 16) & 0xff);
    uint32_t work_item = value & 0xffff;
    if (target <= 0) {
        target = smp_worker_target(sys, work_item);
    }

    smp_deliver_ipi(sync, sys, target, work_item);
    smp_sync_run_worker_jobs(sys, target);
    return true;
}

bool smp_sync_write_ram(rv32_system_t *sys, uint32_t addr,
                        uint32_t value, int size) {
    if (addr < SMP_SYNC_BASE || addr + (uint32_t)size > SMP_SYNC_BASE + 0x40) {
        return false;
    }

    uint8_t *ram = sync_ram(sys);
    if (!ram) {
        return false;
    }

    uint32_t off = addr - RAM_BASE;
    switch (size) {
    case 1:
        ram[off] = (uint8_t)value;
        return true;
    case 2:
        *(uint16_t *)(ram + off) = (uint16_t)value;
        return true;
    case 4:
        *(uint32_t *)(ram + off) = value;
        return true;
    default:
        return false;
    }
}

uint32_t smp_sync_read_ram(rv32_system_t *sys, uint32_t addr, int size) {
    if (addr < SMP_SYNC_BASE || addr + (uint32_t)size > SMP_SYNC_BASE + 0x40) {
        return 0;
    }

    uint8_t *ram = sync_ram(sys);
    if (!ram) {
        return 0;
    }

    uint32_t off = addr - RAM_BASE;
    switch (size) {
    case 1:
        return ram[off];
    case 2:
        return *(uint16_t *)(ram + off);
    case 4:
        return *(uint32_t *)(ram + off);
    default:
        return 0;
    }
}

uint32_t smp_sync_read_mmio(rv32_system_t *sys, uint32_t addr, int size) {
    (void)size;
    if (addr == SMP_IPI_MMIO) {
        return ram_read32(sys, SMP_SYNC_BASE + SMP_OFF_SHARED_DONE);
    }
    return 0;
}

void smp_sync_on_counter_store(smp_sync_t *sync, rv32_system_t *sys,
                               int src_hart, uint32_t counter) {
    if (sys->num_harts <= 1 || src_hart != 0) {
        return;
    }

    int target = smp_worker_target(sys, counter);
    if (target < 0) {
        return;
    }

    smp_deliver_ipi(sync, sys, target, counter);
    smp_sync_run_worker_jobs(sys, target);
}
