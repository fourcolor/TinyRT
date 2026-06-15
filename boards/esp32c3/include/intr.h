/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <stdint.h>
#include "hal.h"
#include "interrupt.h"
#include "logger.h"
#include "port.h"
#include "reg.h"

#define INTERRUPT_PRIO_REG(n) (INTERRUPT_CORE0_CPU_INT_PRI_0_REG + (n)*4)

/* Interrupt hardware source table.
 * This table is defined by ESP32-C3 hardware and the interrupt matrix. */
typedef enum
{
    ETS_WIFI_MAC_INTR_SOURCE = 0,
    ETS_WIFI_MAC_NMI_SOURCE,
    ETS_WIFI_PWR_INTR_SOURCE,
    ETS_WIFI_BB_INTR_SOURCE,
    ETS_BT_MAC_INTR_SOURCE,
    ETS_BT_BB_INTR_SOURCE,
    ETS_BT_BB_NMI_SOURCE,
    ETS_RWBT_INTR_SOURCE,
    ETS_RWBLE_INTR_SOURCE,
    ETS_RWBT_NMI_SOURCE,
    ETS_RWBLE_NMI_SOURCE,
    ETS_I2C_MASTER_SOURCE,
    ETS_SLC0_INTR_SOURCE,
    ETS_SLC1_INTR_SOURCE,
    ETS_APB_CTRL_INTR_SOURCE,
    ETS_UHCI0_INTR_SOURCE,
    ETS_GPIO_INTR_SOURCE,
    ETS_GPIO_NMI_SOURCE,
    ETS_SPI1_INTR_SOURCE,
    ETS_SPI2_INTR_SOURCE,
    ETS_I2S0_INTR_SOURCE,
    ETS_UART0_INTR_SOURCE,
    ETS_UART1_INTR_SOURCE,
    ETS_LEDC_INTR_SOURCE,
    ETS_EFUSE_INTR_SOURCE,
    ETS_TWAI_INTR_SOURCE,
    ETS_USB_SERIAL_JTAG_INTR_SOURCE,
    ETS_RTC_CORE_INTR_SOURCE,
    ETS_RMT_INTR_SOURCE,
    ETS_I2C_EXT0_INTR_SOURCE,
    ETS_TIMER1_INTR_SOURCE,
    ETS_TIMER2_INTR_SOURCE,
    ETS_TG0_T0_LEVEL_INTR_SOURCE,
    ETS_TG0_WDT_LEVEL_INTR_SOURCE,
    ETS_TG1_T0_LEVEL_INTR_SOURCE,
    ETS_TG1_WDT_LEVEL_INTR_SOURCE,
    ETS_CACHE_IA_INTR_SOURCE,
    ETS_SYSTIMER_TARGET0_INTR_SOURCE,
    ETS_SYSTIMER_TARGET1_INTR_SOURCE,
    ETS_SYSTIMER_TARGET2_INTR_SOURCE,
    ETS_SYSTIMER_TARGET0_EDGE_INTR_SOURCE = ETS_SYSTIMER_TARGET0_INTR_SOURCE,
    ETS_SYSTIMER_TARGET1_EDGE_INTR_SOURCE = ETS_SYSTIMER_TARGET1_INTR_SOURCE,
    ETS_SYSTIMER_TARGET2_EDGE_INTR_SOURCE = ETS_SYSTIMER_TARGET2_INTR_SOURCE,
    ETS_SPI_MEM_REJECT_CACHE_INTR_SOURCE = 40,
    ETS_ICACHE_PRELOAD0_INTR_SOURCE,
    ETS_ICACHE_SYNC0_INTR_SOURCE,
    ETS_APB_ADC_INTR_SOURCE,
    ETS_DMA_CH0_INTR_SOURCE,
    ETS_DMA_CH1_INTR_SOURCE,
    ETS_DMA_CH2_INTR_SOURCE,
    ETS_RSA_INTR_SOURCE,
    ETS_AES_INTR_SOURCE,
    ETS_SHA_INTR_SOURCE,
    ETS_FROM_CPU_INTR0_SOURCE,
    ETS_FROM_CPU_INTR1_SOURCE,
    ETS_FROM_CPU_INTR2_SOURCE,
    ETS_FROM_CPU_INTR3_SOURCE,
    ETS_ASSIST_DEBUG_INTR_SOURCE,
    ETS_DMA_APBPERI_PMS_INTR_SOURCE,
    ETS_CORE0_IRAM0_PMS_INTR_SOURCE,
    ETS_CORE0_DRAM0_PMS_INTR_SOURCE,
    ETS_CORE0_PIF_PMS_INTR_SOURCE,
    ETS_CORE0_PIF_PMS_SIZE_INTR_SOURCE,
    ETS_BAK_PMS_VIOLATE_INTR_SOURCE,
    ETS_CACHE_CORE0_ACS_INTR_SOURCE,
    ETS_MAX_INTR_SOURCE,
} periph_interrupt_t;

typedef enum
{
    INTR_TYPE_LEVEL = 0,
    INTR_TYPE_EDGE,
} intr_type;

static inline void interrupt_intc_route(int intr_src, int intr_num)
{
    REG_WRITE(DR_REG_INTERRUPT_BASE + 4 * intr_src, intr_num);
}

static inline uint32_t get_interrupt_unmask(void)
{
    return REG_READ(INTERRUPT_CORE0_CPU_INT_ENABLE_REG);
}

static inline intr_type get_intr_type(int rv_int_num)
{
    uint32_t intr_type_reg = REG_READ(INTERRUPT_CORE0_CPU_INT_TYPE_REG);

    return (intr_type_reg & (1 << rv_int_num)) ? INTR_TYPE_EDGE : INTR_TYPE_LEVEL;
}

static inline int int_get_priority(int rv_int_num)
{
    return REG_READ(INTERRUPT_PRIO_REG(rv_int_num));
}

static inline void mtvec_set(void (*handler)(void))
{
    uintptr_t v = (uintptr_t)handler | 1;

    LOG_DEBUG("mtvec=0x%08lx\n", v);
    __asm__ volatile("csrw mtvec, %0" : : "r"(v) : "memory");
}

static inline void mie_set(uint32_t mask)
{
    __asm__ volatile("csrs mie, %0" : : "r"(mask) : "memory");
}

static inline void ei(void)
{
    arch_interrupt_enable();
}

static inline void di(void)
{
    arch_interrupt_disable();
}
