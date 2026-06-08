#ifndef SMP_PROGRAM_H
#define SMP_PROGRAM_H

#include <stdint.h>

/* Worker hart RV32I code linked into the embedded image at 0x80000030. */
#define SMP_WORKER_WAIT     0x80000030u
#define SMP_WORKER_POLL     0x80000034u
#define SMP_WORKER_SPIN     0x80000050u
#define SMP_WORKER_HANDLE   0x80000054u
#define SMP_WORKER_RELEASE  0x80000088u
#define SMP_WORKER_END      0x80000090u

/* Encodings from examples/test.s (worker_wait at 0x80000030). */
static const uint32_t smp_worker_code[] = {
    0x80002437,  /* 0x80000030: lui   s0, 0x80002 */
    0x01040293,  /* 0x80000034: addi  t0, s0, 16 */
    0x00221e13,  /* 0x80000038: slli  t3, tp, 2 */
    0x01c282b3,  /* 0x8000003c: add   t0, t0, t3 */
    0x0002a303,  /* 0x80000040: lw    t1, 0(t0) */
    0x00030663,  /* 0x80000044: beqz  t1, worker_spin */
    0x0002a023,  /* 0x80000048: sw    zero, 0(t0) */
    0x008000ef,  /* 0x8000004c: jal   worker_handle */
    0xfe5ff06f,  /* 0x80000050: j     worker_poll */
    0x00042283,  /* 0x80000054: lw    t0, 0(s0) */
    0xfe029ee3,  /* 0x80000058: bnez  t0, worker_handle */
    0x00120293,  /* 0x8000005c: addi  t0, tp, 1 */
    0x00542023,  /* 0x80000060: sw    t0, 0(s0) */
    0x00842383,  /* 0x80000064: lw    t2, 8(s0) */
    0x02038063,  /* 0x80000068: beqz  t2, worker_release */
    0x00442303,  /* 0x8000006c: lw    t1, 4(s0) */
    0x00042423,  /* 0x80000070: sw    zero, 8(s0) */
    0x00030493,  /* 0x80000074: mv    s1, t1 */
    0x00c42383,  /* 0x80000078: lw    t2, 12(s0) */
    0x00138393,  /* 0x8000007c: addi  t2, t2, 1 */
    0x00742623,  /* 0x80000080: sw    t2, 12(s0) */
    0x00038913,  /* 0x80000084: mv    s2, t2 */
    0x00042023,  /* 0x80000088: sw    zero, 0(s0) */
    0xfa9ff06f,  /* 0x8000008c: j     worker_poll */
};

#define SMP_WORKER_CODE_WORDS (sizeof(smp_worker_code) / sizeof(smp_worker_code[0]))

#endif /* SMP_PROGRAM_H */
