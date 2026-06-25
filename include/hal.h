#pragma once
#include <stdint.h>
#include "reg.h"
#include "board.h"
#include "rtos_config.h"

#define BIT(x) ((uint32_t)(1U << (x)))

#define REG(base) ((volatile uint32_t *)(base))

/* Peripheral base addresses */
#define C3_GPIO 0x60004000UL
#define C3_IO_MUX 0x60009000UL
#define C3_RTCCNTL 0x60008000UL
#define C3_TIMERGROUP0 0x6001F000UL
#define C3_TIMERGROUP1 0x60020000UL
#define C3_SYSTEM 0x600c0000UL

/* SYSTEM register indices (offset / 4) */
#define SYSTEM_PERIP_CLK_EN0_IDX 4 /* 0x10, bit 29 = systimer_clk_en */
#define SYSTEM_PERIP_RST_EN0_IDX 6 /* 0x18, bit 29 = systimer_rst    */

/* SYSTIMER: 40 MHz XTAL / 2.5 = 16 MHz → 1 µs = 16 ticks */
#define SYSTIMER_TICKS_PER_US 16u
#define TICK_PERIOD_US 1000u /* 1 ms = 1000 Hz */
#define TICK_PERIOD_TICKS (TICK_PERIOD_US * SYSTIMER_TICKS_PER_US)

/* GPIO */
#define GPIO_OUT_FUNC 341
#define GPIO_OUT_EN 8

struct systimer
{
    volatile uint32_t CONF, UNIT0_OP, UNIT1_OP, UNIT0_LOAD_HI, UNIT0_LOAD_LO, UNIT1_LOAD_HI,
        UNIT1_LOAD_LO, TARGET0_HI, TARGET0_LO, TARGET1_HI, TARGET1_LO, TARGET2_HI, TARGET2_LO,
        TARGET0_CONF, TARGET1_CONF, TARGET2_CONF, UNIT0_VALUE_HI, UNIT0_VALUE_LO, UNIT1_VALUE_HI,
        UNIT1_VALUE_LO, COMP0_LOAD, COMP1_LOAD, COMP2_LOAD, UNIT0_LOAD, UNIT1_LOAD, INT_ENA,
        INT_RAW, INT_CLR, INT_ST, RESERVED0[34], DATE;
};
#define SYSTIMER ((struct systimer *)DR_REG_SYSTIMER_BASE)

/* Software millisecond tick maintained by the SYSTIMER interrupt handler. */
extern volatile uint32_t tick_count;

void wdt_disable(void);
void gpio_output(int pin);
void gpio_write(int pin, int val);
void systimer_init(void);
void systimer_start(void);
uint64_t systimer_get_count(void);
uint32_t systimer_get_tick(void);
int arch_in_isr(void);
