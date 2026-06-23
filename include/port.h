#pragma once

#ifdef ARCH_ARM_CORTEX_M
#include "arch/arm_cortex_m/port.h"
#else
#include "arch/riscv/port.h"
#endif
