#pragma once

#include <stdint.h>

#define ARCH_MSTATUS_MPP_M 0x1800u
#define ARCH_MSTATUS_MPIE 0x80u
#define ARCH_TASK_FRAME_WORDS 33
#define ARCH_TASK_CONTEXT_SIZE 144
#define ARCH_TASK_FRAME_ENTRY_INDEX 31

static inline uint32_t arch_read_gp(void)
{
    uint32_t value;

    __asm__ volatile("mv %0, gp" : "=r"(value));
    return value;
}

static inline uint32_t arch_read_tp(void)
{
    uint32_t value;

    __asm__ volatile("mv %0, tp" : "=r"(value));
    return value;
}

static inline uint32_t *arch_task_init_frame(uint8_t *stack_base, uint32_t stack_size,
                                             void (*entry)(void *), void *arg,
                                             void (*exit_fn)(void))
{
    uintptr_t top = (uintptr_t)stack_base + stack_size;
    top &= ~(uintptr_t)0xf;

    uint32_t *frame = (uint32_t *)(top - ARCH_TASK_CONTEXT_SIZE);

    for (int i = 0; i < ARCH_TASK_FRAME_WORDS; i++)
    {
        frame[i] = 0;
    }

    frame[0] = (uint32_t)exit_fn; /* ra */
    frame[1] = arch_read_gp();    /* gp */
    frame[2] = arch_read_tp();    /* tp */
    frame[8] = (uint32_t)arg;     /* a0 */
    frame[31] = (uint32_t)entry;  /* mepc */
    frame[32] = ARCH_MSTATUS_MPP_M | ARCH_MSTATUS_MPIE;

    return frame;
}

static inline void *arch_task_frame_entry(uint32_t *sp)
{
    return sp != 0 ? (void *)sp[ARCH_TASK_FRAME_ENTRY_INDEX] : 0;
}

static inline void arch_yield(void)
{
    __asm__ volatile("ecall" ::: "memory");
}

static inline void arch_wait_for_interrupt(void)
{
    __asm__ volatile("wfi" ::: "memory");
}

static inline void arch_interrupt_enable(void)
{
    __asm__ volatile("csrsi mstatus, 8" ::: "memory");
}

static inline void arch_interrupt_disable(void)
{
    __asm__ volatile("csrci mstatus, 8" ::: "memory");
}
