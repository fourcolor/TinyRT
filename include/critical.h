#pragma once

#include <stdint.h>
#include "port.h"

typedef uint32_t critical_state_t;

static inline critical_state_t critical_enter(void)
{
    return arch_critical_enter();
}

static inline void critical_exit(critical_state_t state)
{
    arch_critical_exit(state);
}
