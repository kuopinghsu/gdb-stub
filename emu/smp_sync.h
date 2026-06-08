#ifndef SMP_SYNC_H
#define SMP_SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include "rv32_cpu.h"

#define SMP_SYNC_BASE       0x80002000u
#define SMP_IPI_MMIO        0x10000004u

#define SMP_OFF_LOCK        0x00u
#define SMP_OFF_WORK_ITEM   0x04u
#define SMP_OFF_WORK_READY  0x08u
#define SMP_OFF_SHARED_DONE 0x0cu
#define SMP_OFF_IPI_PENDING 0x10u
#define SMP_OFF_NUM_HARTS   0x30u

struct rv32_system;

typedef struct {
    uint32_t num_harts;
} smp_sync_t;

void smp_sync_init(smp_sync_t *sync, int num_harts);
void smp_sync_reset(smp_sync_t *sync, int num_harts);
void smp_sync_ram_init(struct rv32_system *sys);

bool smp_sync_handle_mmio(smp_sync_t *sync, struct rv32_system *sys,
                          uint32_t addr, uint32_t value, int size);
uint32_t smp_sync_read_ram(struct rv32_system *sys, uint32_t addr, int size);
uint32_t smp_sync_read_mmio(struct rv32_system *sys, uint32_t addr, int size);
bool smp_sync_write_ram(struct rv32_system *sys, uint32_t addr,
                        uint32_t value, int size);

void smp_sync_on_counter_store(smp_sync_t *sync, struct rv32_system *sys,
                               int src_hart, uint32_t counter);
void smp_sync_run_worker_jobs(struct rv32_system *sys, int hart_id);
bool smp_sync_worker_should_run(struct rv32_system *sys, int hart_id);

void smp_sync_init_worker_hart(struct rv32_system *sys, rv32_cpu_t *cpu);

#endif /* SMP_SYNC_H */
