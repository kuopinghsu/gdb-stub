#ifndef RV32_SYSTEM_H
#define RV32_SYSTEM_H

#include "rv32_cpu.h"
#include "smp_sync.h"

struct rv32_system {
    rv32_cpu_t harts[RV32_MAX_HARTS];
    smp_sync_t sync;
    int num_harts;
    int focus_hart;
    int stop_hart;
};

typedef struct rv32_system rv32_system_t;

#endif /* RV32_SYSTEM_H */
