#include "hal.h"
#include "intr.h"
#include "csr.h"
#include "systimer_reg.h"
#include "sched.h"
#include "logger.h"
#include "timer.h"

#define MSTATUS_MPIE 0x80u

/* Tick counter definition (declared extern in hal.h) */
volatile uint32_t tick_count;

/* --- Dynamic interrupt dispatch table ------------------------------------- */
static struct intr_handler_t intr_table[32];

/* CPU interrupt allocation bitmap -- moved to module scope so systimer_init
 * can reset it on warm restart (where BSS is not re-zeroed). */
static uint32_t cpu_int_allocated;
static int systimer_cpu_irq;
static int systimer_initialized;
static int systimer_started;
static volatile uint32_t trap_nesting;

static void SysTick_Handler(void *arg);

static inline void io_sync(void)
{
    __asm__ volatile("fence iorw, iorw" ::: "memory");
}

int arch_in_isr(void)
{
    return trap_nesting != 0;
}

void intr_register(int intc_id, struct intr_handler_t *handler)
{
    intr_table[intc_id].fn = handler->fn;
    intr_table[intc_id].arg = handler->arg;
}

/* C-level trap handler called from _isr in startup.c.
 * Receives task_sp (pointer to trap frame on task's stack) and mcause.
 * Returns the frame SP to restore from (same = no switch, different = switch).
 * When current_task is NULL (during boot), saves to boot stack and returns
 * task_sp unchanged (no context switch, continue on boot stack). */
uint32_t *do_trap(uint32_t *task_sp, uint32_t cause)
{
    uint32_t *return_sp;

    trap_nesting++;

    /* Before task_start_first(), traps run on the boot stack. Do not save that
     * frame into the selected task, or first task startup can resume inside
     * rtos_start() instead of the task entry. */
    if (!scheduler.started)
    {
        if (cause >> 31)
        {
            int id = cause & 0xff;

            if (id < 0 || id >= 32)
            {
                LOG_WARN("invalid intr id %d before scheduler cause=0x%08lx\n", id, cause);
                trap_nesting--;
                return task_sp;
            }

            if (systimer_initialized && id == systimer_cpu_irq)
            {
                SysTick_Handler(0);
            }
            else if (intr_table[id].fn)
            {
                intr_table[id].fn(intr_table[id].arg);
            }
            else
            {
                LOG_WARN("unhandled intr %d before scheduler\n", id);
            }
        }

        trap_nesting--;
        return task_sp;
    }

    /* Save current task's frame pointer so context can be resumed */
    if (scheduler.current_task != NULL)
    {
        scheduler.current_task->sp = task_sp;
    }

    int systick_fired = 0;

    if (cause >> 31)
    {
        /* Asynchronous interrupt */
        int id = cause & 0xff;

        if (id < 0 || id >= 32)
        {
            LOG_WARN("invalid intr id %d cause=0x%08lx\n", id, cause);
            trap_nesting--;
            return task_sp;
        }

        if (systimer_initialized && id == systimer_cpu_irq)
        {
            SysTick_Handler(0);
            systick_fired = 1;
        }
        else if (intr_table[id].fn)
        {
            intr_table[id].fn(intr_table[id].arg);
        }
        else
        {
            LOG_WARN("unhandled intr %d\n", id);
        }
    }
    else
    {
        /* Synchronous exception */
        int code = cause & 0x7fffffff;

        if (code == 11)
        {
            /* M-mode ecall -- Advance mepc past ecall so mret resumes at the
             * next instruction. Update both frame (authoritative copy) and
             * CSR (visible to debugger/mret). */
            uint32_t *frame = (uint32_t *)task_sp;
            uint32_t new_epc = frame[31] + 4; /* frame[31] = mepc */
            frame[31] = new_epc;
            frame[32] |= MSTATUS_MPIE;
            write_csr(CSR_MEPC, new_epc);
        }
        else
        {
            LOG_ERROR("EXCEPTION cause=0x%08lx\n", cause);
            for (;;)
                ;
        }
    }

    if (scheduler.current_task == NULL)
    {
        trap_nesting--;
        return task_sp;
    }

    sched_wake_expired();
    timer_run_expired();

    if (scheduler.current_task->state != TASK_RUNNING || sched_current_is_idle())
    {
        scheduler.current_task = sched_pick_next();
    }
    else if (systick_fired && sched_on_tick())
    {
        if (sched_is_locked())
        {
            sched_set_pending();
        }
        else
        {
            scheduler.current_task = sched_pick_next();
        }
    }

    if (!systick_fired)
    {
        sched_try_reschedule_from_trap();
    }

    return_sp = scheduler.current_task->sp;
    trap_nesting--;
    return return_sp;
}

void wdt_disable(void)
{
    /* RTC WDT */
    REG(C3_RTCCNTL)[42] = 0x50D83AA1UL; /* 0xA8 write-protect unlock */
    REG(C3_RTCCNTL)[36] = 0;            /* 0x90 WDTCONFIG0 */
    REG(C3_RTCCNTL)[35] = 0;            /* 0x8C */

    /* Super WDT */
    REG(C3_RTCCNTL)[44] = 0x8F1D312AUL; /* 0xB0 SWD unlock */
    REG(C3_RTCCNTL)[43] |= BIT(31);     /* 0xAC SWD_DISABLE */
    REG(C3_RTCCNTL)[45] = 0;            /* 0xB4 */

    /* TIMG0 WDT: unlock (TIMG_WDTPROTECT offset 0xA4 = index 41), disable, re-lock */
    REG(C3_TIMERGROUP0)[41] = 0x50D83AA1UL;
    REG(C3_TIMERGROUP0)[18] = 0; /* TIMG_WDTCONFIG0 offset 0x48 */
    REG(C3_TIMERGROUP0)[41] = 0;

    /* TIMG1 WDT */
    REG(C3_TIMERGROUP1)[41] = 0x50D83AA1UL;
    REG(C3_TIMERGROUP1)[18] = 0;
    REG(C3_TIMERGROUP1)[41] = 0;
}

void gpio_output(int pin)
{
    /* IO_MUX: MCU_SEL = 1 (route through GPIO Matrix) */
    volatile uint32_t *io_mux = (volatile uint32_t *)(C3_IO_MUX + 0x04 + pin * 4);
    *io_mux = (*io_mux & ~(0x7u << 12)) | (1u << 12);

    /* GPIO Matrix: output source = SIG_GPIO_OUT (128 = CPU-driven) */
    REG(C3_GPIO)
    [GPIO_OUT_FUNC + pin] = 128U;

    /* Enable output */
    REG(C3_GPIO)
    [GPIO_OUT_EN] &= ~BIT(pin);
    REG(C3_GPIO)
    [GPIO_OUT_EN] |= BIT(pin);
}

void gpio_write(int pin, int val)
{
    REG(C3_GPIO)
    [1] &= ~BIT(pin);
    REG(C3_GPIO)
    [1] |= (val ? 1U : 0U) << pin;
}
uint64_t systimer_get_count(void)
{
    uint32_t lo;
    uint32_t lo_start;
    uint32_t hi;
    uint32_t timeout = 1000000;

    /* Match Espressif HAL:
     * 1. trigger counter snapshot/update
     * 2. wait until VALUE_VALID is set
     * 3. read LO-HI-LO until LO is stable, so an ISR between reads cannot
     *    return a torn 64-bit value. */
    SYSTIMER->UNIT0_OP |= SYSTIMER_TIMER_UNIT0_UPDATE;
    while ((SYSTIMER->UNIT0_OP & SYSTIMER_TIMER_UNIT0_VALUE_VALID) == 0)
    {
        if (--timeout == 0)
        {
            LOG_WARN("systimer snapshot timeout op=0x%08lx conf=0x%08lx\n", SYSTIMER->UNIT0_OP,
                     SYSTIMER->CONF);
            return 0;
        }
    }

    lo_start = SYSTIMER->UNIT0_VALUE_LO;
    do
    {
        lo = lo_start;
        hi = SYSTIMER->UNIT0_VALUE_HI & SYSTIMER_TIMER_UNIT0_VALUE_HI;
        lo_start = SYSTIMER->UNIT0_VALUE_LO;
    } while (lo_start != lo);

    return ((uint64_t)hi << 32) | lo;
}

uint32_t systimer_get_tick(void)
{
    return (uint32_t)(systimer_get_count() / TICK_PERIOD_TICKS);
}

int cpu_alloc_interrupt(uint8_t prio /* 1..15 */)
{
    for (uint8_t no = 1; no < 31; no++)
    {
        if (cpu_int_allocated & BIT(no))
            continue;                 // Used, try the next one
        cpu_int_allocated |= BIT(no); // Claim this one
        LOG_DEBUG("cpu irq claim %d\n", no);
        REG_WRITE(INTERRUPT_CORE0_CPU_INT_THRESH_REG, 0);
        REG_WRITE(INTERRUPT_CORE0_CPU_INT_CLEAR_REG, BIT(no));
        REG_WRITE(INTC_INT_PRIO_REG(no), prio); // CPU_INT_PRI_N
        REG_WRITE(INTERRUPT_CORE0_CPU_INT_TYPE_REG,
                  REG_READ(INTERRUPT_CORE0_CPU_INT_TYPE_REG) & ~BIT(no));
        REG_SET_BIT(INTERRUPT_CORE0_CPU_INT_ENABLE_REG, no); // CPU_INT_ENA
        (void)REG_READ(INTERRUPT_CORE0_CPU_INT_ENABLE_REG);
        io_sync();
        LOG_DEBUG("cpu irq enable %d\n", no);
        LOG_DEBUG("cpu irq prio %d\n", no);
        LOG_DEBUG("allocated CPU IRQ %d, prio %d\n", no, prio);
        return no;
    }
    return -1;
}

static void SysTick_Handler(void *arg)
{
    (void)arg;

    /* Clear target 0 interrupt flag (WTC), then read-back to flush APB write */
    SYSTIMER->INT_CLR = 7U;
    (void)SYSTIMER->INT_ST;
    io_sync();
    tick_count = systimer_get_tick();
}

void systimer_init()
{
    void *caller;

    LOG_DEBUG("systimer_init\n");

    if (systimer_initialized)
    {
        caller = __builtin_return_address(0);
        SYSTIMER->INT_CLR = 7U;
        LOG_WARN("systimer_init called twice caller=%p irq=%d\n", caller, systimer_cpu_irq);
        return;
    }

    /* Reset allocation state and INTC for warm-restart robustness.
     * On a CPU-only WDT reset the BSS is not re-zeroed, so stale bits
     * from a previous boot must be cleared manually. */
    cpu_int_allocated = 0;
    REG_WRITE(INTERRUPT_CORE0_CPU_INT_ENABLE_REG, 0);
    REG_WRITE(INTERRUPT_CORE0_CPU_INT_CLEAR_REG, 0xffffffffu);
    REG_WRITE(INTERRUPT_CORE0_CPU_INT_THRESH_REG, 0);
    (void)REG_READ(INTERRUPT_CORE0_CPU_INT_ENABLE_REG);
    io_sync();

    /* Enable SYSTIMER APB and internal clocks */
    REG(C3_SYSTEM)[SYSTEM_PERIP_CLK_EN0_IDX] |= BIT(29);
    REG(C3_SYSTEM)[SYSTEM_PERIP_RST_EN0_IDX] &= ~BIT(29);
    (void)REG(C3_SYSTEM)[SYSTEM_PERIP_CLK_EN0_IDX];
    io_sync();
    LOG_DEBUG("systimer clock\n");

    SYSTIMER->INT_ENA = 0;
    SYSTIMER->INT_CLR = 7U;
    (void)SYSTIMER->INT_ENA;
    io_sync();

    SYSTIMER->CONF &= ~SYSTIMER_TARGET0_WORK_EN;
    SYSTIMER->CONF |= SYSTIMER_CLK_EN | SYSTIMER_TIMER_UNIT0_WORK_EN;
    (void)SYSTIMER->CONF;
    io_sync();
    LOG_DEBUG("systimer conf\n");

    SYSTIMER->UNIT0_LOAD_HI = 0;
    SYSTIMER->UNIT0_LOAD_LO = 0;
    SYSTIMER->UNIT0_LOAD = 1;
    (void)SYSTIMER->UNIT0_LOAD;
    io_sync();

    /* TRM 10.5.3: configure period mode
     * Step 1: write period with PERIOD_MODE cleared */
    SYSTIMER->TARGET0_CONF = TICK_PERIOD_TICKS;
    (void)SYSTIMER->TARGET0_CONF;
    io_sync();
    LOG_DEBUG("systimer period\n");

    /* Step 2: sync period value into comparator */
    SYSTIMER->COMP0_LOAD = SYSTIMER_TIMER_COMP0_LOAD;
    (void)SYSTIMER->COMP0_LOAD;
    io_sync();
    LOG_DEBUG("systimer load\n");

    /* Step 3: set PERIOD_MODE */
    SYSTIMER->TARGET0_CONF |= SYSTIMER_TARGET0_PERIOD_MODE;
    (void)SYSTIMER->TARGET0_CONF;
    io_sync();
    LOG_DEBUG("systimer period mode\n");

    /* Allocate a CPU interrupt line */
    int no = cpu_alloc_interrupt(1);
    if (no < 0)
        return;
    systimer_cpu_irq = no;

    /* Register handler before enabling interrupt source */
    intr_table[no].fn = SysTick_Handler;

    /* Route SYSTIMER_TARGET0 → allocated CPU line */
    interrupt_intc_route(ETS_SYSTIMER_TARGET0_INTR_SOURCE, no);
    LOG_DEBUG("systimer routed to CPU IRQ %d\n", no);

    tick_count = 0;
    systimer_started = 0;
    systimer_initialized = 1;
}

void systimer_start(void)
{
    uint32_t conf;

    if (systimer_started)
    {
        return;
    }

    /* Counter 0 is already running from systimer_init(). Only arm target0 and
     * its interrupt here, after the first task has reached a blocking point. */
    SYSTIMER->INT_CLR = 7U;
    (void)SYSTIMER->INT_ST;
    io_sync();

    SYSTIMER->COMP0_LOAD = SYSTIMER_TIMER_COMP0_LOAD;
    (void)SYSTIMER->COMP0_LOAD;
    io_sync();

    systimer_started = 1;

    SYSTIMER->INT_ENA = SYSTIMER_TARGET0_INT_ENA;
    (void)SYSTIMER->INT_ENA;
    io_sync();

    SYSTIMER->INT_CLR = 7U;
    (void)SYSTIMER->INT_ST;
    io_sync();

    conf = SYSTIMER->CONF;
    conf |= SYSTIMER_CLK_EN | SYSTIMER_TIMER_UNIT0_WORK_EN | SYSTIMER_TARGET0_WORK_EN;
    SYSTIMER->CONF = conf;
}
