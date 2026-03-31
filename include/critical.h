#pragma once

#include <stdint.h>

typedef uint32_t critical_state_t;

static inline critical_state_t critical_enter(void)
{
    critical_state_t mstatus;

    __asm__ volatile("csrr %0, mstatus\n"
                     "csrci mstatus, 8"
                     : "=r"(mstatus)
                     :
                     : "memory");
    return mstatus;
}

static inline void critical_exit(critical_state_t mstatus)
{
    __asm__ volatile("csrw mstatus, %0" : : "r"(mstatus) : "memory");
}
