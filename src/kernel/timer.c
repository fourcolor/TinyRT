#include "timer.h"
#include "arch_port.h"
#include "critical.h"
#include "hal.h"

static list_head_t timer_list;
static int timer_initialized;
static int timer_tick_started;

static void timer_insert_locked(timer_t *timer)
{
    list_head_t *pos;

    list_for_each(pos, &timer_list)
    {
        timer_t *queued = list_entry(pos, timer_t, node);

        if ((int32_t)(timer->deadline - queued->deadline) < 0)
        {
            list_add_tail(&timer->node, pos);
            return;
        }
    }

    list_add_tail(&timer->node, &timer_list);
}

void timer_init(void)
{
    if (timer_initialized)
    {
        return;
    }

    INIT_LIST_HEAD(&timer_list);
    systimer_init();
    timer_initialized = 1;
}

void timer_start_tick(void)
{
    uint32_t state;

    if (timer_tick_started)
    {
        return;
    }

    state = critical_enter();
    if (!timer_tick_started)
    {
        systimer_start();
        timer_tick_started = 1;
    }
    critical_exit(state);
}

uint32_t timer_ticks(void)
{
    return tick_count;
}

uint64_t timer_cycles(void)
{
    return systimer_get_count();
}

uint64_t timer_us(void)
{
    return timer_cycles() / SYSTIMER_TICKS_PER_US;
}

uint32_t timer_us_to_ticks(uint64_t us)
{
    uint64_t ticks;

    if (us == 0)
    {
        return 0;
    }

    ticks = (us + TICK_PERIOD_US - 1u) / TICK_PERIOD_US;
    if (ticks > UINT32_MAX)
    {
        return UINT32_MAX;
    }

    return (uint32_t)ticks;
}

uint32_t timer_ms_to_ticks(uint64_t ms)
{
    return timer_us_to_ticks(ms * 1000u);
}

uint32_t timer_sec_to_ticks(uint64_t sec)
{
    return timer_ms_to_ticks(sec * 1000u);
}

int timer_expired(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

void timer_setup(timer_t *timer, timer_callback_t callback, void *arg)
{
    if (timer == 0)
    {
        return;
    }

    INIT_LIST_HEAD(&timer->node);
    timer->deadline = 0;
    timer->period = 0;
    timer->callback = callback;
    timer->arg = arg;
    timer->active = 0;
}

void timer_start(timer_t *timer, uint32_t delay_ticks, uint32_t period_ticks)
{
    uint32_t state;

    if (timer == 0 || timer->callback == 0)
    {
        return;
    }

    state = critical_enter();

    if (timer->active)
    {
        list_del_init(&timer->node);
    }

    timer->deadline = timer_ticks() + delay_ticks;
    timer->period = period_ticks;
    timer->active = 1;
    timer_insert_locked(timer);

    critical_exit(state);
}

void timer_stop(timer_t *timer)
{
    uint32_t state;

    if (timer == 0)
    {
        return;
    }

    state = critical_enter();

    if (timer->active)
    {
        list_del_init(&timer->node);
        timer->active = 0;
    }

    critical_exit(state);
}

err_t timer_destroy(timer_t *timer)
{
    uint32_t state;

    if (timer == 0)
    {
        return ERR_INVAL;
    }

    state = critical_enter();

    if (timer->active)
    {
        list_del_init(&timer->node);
    }
    else if (!list_empty(&timer->node))
    {
        list_del_init(&timer->node);
    }

    timer->deadline = 0;
    timer->period = 0;
    timer->callback = 0;
    timer->arg = 0;
    timer->active = 0;

    critical_exit(state);
    return ERR_OK;
}

int timer_active(timer_t *timer)
{
    return timer != 0 && timer->active;
}

void timer_run_expired(void)
{
    uint32_t state;

    for (;;)
    {
        timer_t *timer;
        timer_callback_t callback;
        void *arg;
        uint32_t now;

        state = critical_enter();

        if (list_empty(&timer_list))
        {
            critical_exit(state);
            return;
        }

        timer = list_first_entry(&timer_list, timer_t, node);
        now = timer_ticks();
        if (!timer_expired(now, timer->deadline))
        {
            critical_exit(state);
            return;
        }

        list_del_init(&timer->node);
        callback = timer->callback;
        arg = timer->arg;

        if (timer->period != 0)
        {
            timer->deadline += timer->period;
            timer_insert_locked(timer);
        }
        else
        {
            timer->active = 0;
        }

        critical_exit(state);
        callback(arg);
    }
}
