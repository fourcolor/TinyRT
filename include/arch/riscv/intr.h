/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "reg.h"
#include "hal.h"
#include "logger.h"
#include "port.h"
#define INTERRUPT_PRIO_REG(n) (INTERRUPT_CORE0_CPU_INT_PRI_0_REG + (n)*4)

// Interrupt hardware source table
// This table is decided by hardware, don't touch this.
typedef enum
{
    ETS_WIFI_MAC_INTR_SOURCE = 0, /**< interrupt of WiFi MAC, level*/
    ETS_WIFI_MAC_NMI_SOURCE,  /**< interrupt of WiFi MAC, NMI, use if MAC have bug to fix in NMI*/
    ETS_WIFI_PWR_INTR_SOURCE, /**< */
    ETS_WIFI_BB_INTR_SOURCE,  /**< interrupt of WiFi BB, level, we can do some calibartion*/
    ETS_BT_MAC_INTR_SOURCE,   /**< will be cancelled*/
    ETS_BT_BB_INTR_SOURCE,    /**< interrupt of BT BB, level*/
    ETS_BT_BB_NMI_SOURCE,     /**< interrupt of BT BB, NMI, use if BB have bug to fix in NMI*/
    ETS_RWBT_INTR_SOURCE,     /**< interrupt of RWBT, level*/
    ETS_RWBLE_INTR_SOURCE,    /**< interrupt of RWBLE, level*/
    ETS_RWBT_NMI_SOURCE,      /**< interrupt of RWBT, NMI, use if RWBT have bug to fix in NMI*/
    ETS_RWBLE_NMI_SOURCE,     /**< interrupt of RWBLE, NMI, use if RWBT have bug to fix in NMI*/
    ETS_I2C_MASTER_SOURCE,    /**< interrupt of I2C Master, level*/
    ETS_SLC0_INTR_SOURCE,     /**< interrupt of SLC0, level*/
    ETS_SLC1_INTR_SOURCE,     /**< interrupt of SLC1, level*/
    ETS_APB_CTRL_INTR_SOURCE, /**< interrupt of APB ctrl, ?*/
    ETS_UHCI0_INTR_SOURCE,    /**< interrupt of UHCI0, level*/
    ETS_GPIO_INTR_SOURCE,     /**< interrupt of GPIO, level*/
    ETS_GPIO_NMI_SOURCE,      /**< interrupt of GPIO, NMI*/
    ETS_SPI1_INTR_SOURCE,  /**< interrupt of SPI1, level, SPI1 is for flash read/write, do not use
                              this*/
    ETS_SPI2_INTR_SOURCE,  /**< interrupt of SPI2, level*/
    ETS_I2S0_INTR_SOURCE,  /**< interrupt of I2S0, level*/
    ETS_UART0_INTR_SOURCE, /**< interrupt of UART0, level*/
    ETS_UART1_INTR_SOURCE, /**< interrupt of UART1, level*/
    ETS_LEDC_INTR_SOURCE,  /**< interrupt of LED PWM, level*/
    ETS_EFUSE_INTR_SOURCE, /**< interrupt of efuse, level, not likely to use*/
    ETS_TWAI_INTR_SOURCE,  /**< interrupt of can, level*/
    ETS_USB_SERIAL_JTAG_INTR_SOURCE, /**< interrupt of USJ, level*/
    ETS_RTC_CORE_INTR_SOURCE,        /**< interrupt of rtc core, level, include rtc watchdog*/
    ETS_RMT_INTR_SOURCE,             /**< interrupt of remote controller, level*/
    ETS_I2C_EXT0_INTR_SOURCE,        /**< interrupt of I2C controller1, level*/
    ETS_TIMER1_INTR_SOURCE,
    ETS_TIMER2_INTR_SOURCE,
    ETS_TG0_T0_LEVEL_INTR_SOURCE,     /**< interrupt of TIMER_GROUP0, TIMER0, level*/
    ETS_TG0_WDT_LEVEL_INTR_SOURCE,    /**< interrupt of TIMER_GROUP0, WATCH DOG, level*/
    ETS_TG1_T0_LEVEL_INTR_SOURCE,     /**< interrupt of TIMER_GROUP1, TIMER0, level*/
    ETS_TG1_WDT_LEVEL_INTR_SOURCE,    /**< interrupt of TIMER_GROUP1, WATCHDOG, level*/
    ETS_CACHE_IA_INTR_SOURCE,         /**< interrupt of Cache Invalid Access, LEVEL*/
    ETS_SYSTIMER_TARGET0_INTR_SOURCE, /**< interrupt of system timer 0 */
    ETS_SYSTIMER_TARGET1_INTR_SOURCE, /**< interrupt of system timer 1 */
    ETS_SYSTIMER_TARGET2_INTR_SOURCE, /**< interrupt of system timer 2 */
    ETS_SYSTIMER_TARGET0_EDGE_INTR_SOURCE =
        ETS_SYSTIMER_TARGET0_INTR_SOURCE, /**< use ETS_SYSTIMER_TARGET0_INTR_SOURCE */
    ETS_SYSTIMER_TARGET1_EDGE_INTR_SOURCE =
        ETS_SYSTIMER_TARGET1_INTR_SOURCE, /**< use ETS_SYSTIMER_TARGET1_INTR_SOURCE */
    ETS_SYSTIMER_TARGET2_EDGE_INTR_SOURCE =
        ETS_SYSTIMER_TARGET2_INTR_SOURCE, /**< use ETS_SYSTIMER_TARGET2_INTR_SOURCE */
    ETS_SPI_MEM_REJECT_CACHE_INTR_SOURCE =
        40, /**< interrupt of SPI0 Cache access and SPI1 access rejected, LEVEL*/
    ETS_ICACHE_PRELOAD0_INTR_SOURCE, /**< interrupt of ICache perload operation, LEVEL*/
    ETS_ICACHE_SYNC0_INTR_SOURCE,    /**< interrupt of instruction cache sync done, LEVEL*/
    ETS_APB_ADC_INTR_SOURCE,         /**< interrupt of APB ADC, LEVEL*/
    ETS_DMA_CH0_INTR_SOURCE,         /**< interrupt of general DMA channel 0, LEVEL*/
    ETS_DMA_CH1_INTR_SOURCE,         /**< interrupt of general DMA channel 1, LEVEL*/
    ETS_DMA_CH2_INTR_SOURCE,         /**< interrupt of general DMA channel 2, LEVEL*/
    ETS_RSA_INTR_SOURCE,             /**< interrupt of RSA accelerator, level*/
    ETS_AES_INTR_SOURCE,             /**< interrupt of AES accelerator, level*/
    ETS_SHA_INTR_SOURCE,             /**< interrupt of SHA accelerator, level*/
    ETS_FROM_CPU_INTR0_SOURCE,
    /**< interrupt0 generated from a CPU, level*/ /* Used for FreeRTOS */
    ETS_FROM_CPU_INTR1_SOURCE,
    /**< interrupt1 generated from a CPU, level*/ /* Used for FreeRTOS */
    ETS_FROM_CPU_INTR2_SOURCE,                    /**< interrupt2 generated from a CPU, level*/
    ETS_FROM_CPU_INTR3_SOURCE,                    /**< interrupt3 generated from a CPU, level*/
    ETS_ASSIST_DEBUG_INTR_SOURCE,                 /**< interrupt of Assist debug module, LEVEL*/
    ETS_DMA_APBPERI_PMS_INTR_SOURCE,
    ETS_CORE0_IRAM0_PMS_INTR_SOURCE,
    ETS_CORE0_DRAM0_PMS_INTR_SOURCE,
    ETS_CORE0_PIF_PMS_INTR_SOURCE,
    ETS_CORE0_PIF_PMS_SIZE_INTR_SOURCE,
    ETS_BAK_PMS_VIOLATE_INTR_SOURCE,
    ETS_CACHE_CORE0_ACS_INTR_SOURCE,
    ETS_MAX_INTR_SOURCE,
} periph_interrupt_t;

static const char *const esp_isr_names[] = {
    [0] = "WIFI_MAC",
    [1] = "WIFI_MAC_NMI",
    [2] = "WIFI_PWR",
    [3] = "WIFI_BB",
    [4] = "BT_MAC",
    [5] = "BT_BB",
    [6] = "BT_BB_NMI",
    [7] = "RWBT",
    [8] = "RWBLE",
    [9] = "RWBT_NMI",
    [10] = "RWBLE_NMI",
    [11] = "I2C_MASTER",
    [12] = "SLC0",
    [13] = "SLC1",
    [14] = "APB_CTRL",
    [15] = "UHCI0",
    [16] = "GPIO",
    [17] = "GPIO_NMI",
    [18] = "SPI1",
    [19] = "SPI2",
    [20] = "I2S0",
    [21] = "UART0",
    [22] = "UART1",
    [23] = "LEDC",
    [24] = "EFUSE",
    [25] = "TWAI",
    [26] = "USB",
    [27] = "RTC_CORE",
    [28] = "RMT",
    [29] = "I2C_EXT0",
    [30] = "TIMER1",
    [31] = "TIMER2",
    [32] = "TG0_T0_LEVEL",
    [33] = "TG0_WDT_LEVEL",
    [34] = "TG1_T0_LEVEL",
    [35] = "TG1_WDT_LEVEL",
    [36] = "CACHE_IA",
    [37] = "SYSTIMER_TARGET0_EDGE",
    [38] = "SYSTIMER_TARGET1_EDGE",
    [39] = "SYSTIMER_TARGET2_EDGE",
    [40] = "SPI_MEM_REJECT_CACHE",
    [41] = "ICACHE_PRELOAD0",
    [42] = "ICACHE_SYNC0",
    [43] = "APB_ADC",
    [44] = "DMA_CH0",
    [45] = "DMA_CH1",
    [46] = "DMA_CH2",
    [47] = "RSA",
    [48] = "AES",
    [49] = "SHA",
    [50] = "FROM_CPU_INTR0",
    [51] = "FROM_CPU_INTR1",
    [52] = "FROM_CPU_INTR2",
    [53] = "FROM_CPU_INTR3",
    [54] = "ASSIST_DEBUG",
    [55] = "DMA_APBPERI_PMS",
    [56] = "CORE0_IRAM0_PMS",
    [57] = "CORE0_DRAM0_PMS",
    [58] = "CORE0_PIF_PMS",
    [59] = "CORE0_PIF_PMS_SIZE",
    [60] = "BAK_PMS_VIOLATE",
    [61] = "CACHE_CORE0_ACS",
};

struct intr_handler_t
{
    void (*fn)(void *); // User-defined handler function
    void *arg;          // User-defined handler function param
};

typedef enum
{
    INTR_TYPE_LEVEL = 0,
    INTR_TYPE_EDGE
} intr_type;

static inline void interrupt_intc_route(int intr_src, int intr_num)
{
    REG_WRITE(DR_REG_INTERRUPT_BASE + 4 * intr_src, intr_num);
}

static inline uint32_t get_interrupt_unmask()
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
/* Install a trap handler into mtvec (direct mode, bit 0 = 0). */
static inline void mtvec_set(void (*handler)(void))
{
    uintptr_t v = (uintptr_t)handler | 1;
    LOG_DEBUG("mtvec=0x%08lx\n", v);
    __asm__ volatile("csrw mtvec, %0" : : "r"(v) : "memory");
}

/* Set bits in mie (machine interrupt enable). */
static inline void mie_set(uint32_t mask)
{
    __asm__ volatile("csrs mie, %0" : : "r"(mask) : "memory");
}

/* Global interrupt enable / disable (mstatus.MIE). */
static inline void ei(void)
{
    arch_interrupt_enable();
}
static inline void di(void)
{
    arch_interrupt_disable();
}

/* --- Dynamic interrupt handler registration --------------------------------
 * Handlers are called from do_trap with interrupts disabled (MIE=0).
 * Register before calling ei(). Unregister by passing NULL.             */
void intr_register(int cpu_line, struct intr_handler_t *handler);
