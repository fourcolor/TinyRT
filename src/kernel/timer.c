#include "timer_private.h"
#include "port.h"
#include "critical.h"
#include "error.h"
#include "hal.h"
#include "handle_private.h"
#include "list.h"
#include "malloc.h"

typedef struct timer
{
    list_node_t node;
    uint32_t deadline;
    uint32_t period;
    timer_callback_t callback;
    void *arg;
    int active;
} timer_t;

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

static err_t timer_lookup(trt_handle_t handle, uint32_t rights, timer_t **out)
{
    void *object;
    err_t result;

    result = trt_handle_lookup(handle, TRT_OBJ_TIMER, rights, &object);
    if (result != ERR_OK)
    {
        return result;
    }

    *out = object;
    return ERR_OK;
}

static void timer_setup_obj(timer_t *timer, timer_callback_t callback, void *arg)
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

trt_handle_t trt_timer_create(timer_callback_t callback, void *arg)
{
    timer_t *timer;
    trt_handle_t handle;

    if (callback == 0)
    {
        return TRT_HANDLE_INVALID;
    }

    timer = malloc(sizeof(*timer));
    if (timer == 0)
    {
        return TRT_HANDLE_INVALID;
    }

    timer_setup_obj(timer, callback, arg);
    if (trt_handle_alloc(timer, TRT_OBJ_TIMER, TRT_RIGHT_READ | TRT_RIGHT_WRITE | TRT_RIGHT_DESTROY,
                         &handle) != ERR_OK)
    {
        free(timer);
        return TRT_HANDLE_INVALID;
    }

    return handle;
}

static void timer_start_ticks_obj(timer_t *timer, uint32_t delay_ticks, uint32_t period_ticks)
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

static void timer_start_ticks(trt_handle_t handle, uint32_t delay_ticks, uint32_t period_ticks)
{
    timer_t *timer;

    if (timer_lookup(handle, TRT_RIGHT_WRITE, &timer) != ERR_OK)
    {
        return;
    }

    timer_start_ticks_obj(timer, delay_ticks, period_ticks);
}

void trt_timer_start(trt_handle_t handle, trt_time_t delay, trt_time_t period)
{
    if (delay.us == TRT_TIME_FOREVER_US || period.us == TRT_TIME_FOREVER_US)
    {
        return;
    }

    timer_start_ticks(handle, timer_us_to_ticks(delay.us), timer_us_to_ticks(period.us));
}

static void timer_stop_obj(timer_t *timer)
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

void trt_timer_stop(trt_handle_t handle)
{
    timer_t *timer;

    if (timer_lookup(handle, TRT_RIGHT_WRITE, &timer) != ERR_OK)
    {
        return;
    }

    timer_stop_obj(timer);
}

static err_t timer_destroy_obj(timer_t *timer)
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

err_t trt_timer_destroy(trt_handle_t handle)
{
    timer_t *timer;
    err_t result;

    if (arch_in_isr())
    {
        return ERR_STATE;
    }

    result = timer_lookup(handle, TRT_RIGHT_DESTROY, &timer);
    if (result != ERR_OK)
    {
        return result;
    }

    result = timer_destroy_obj(timer);
    if (result != ERR_OK)
    {
        return result;
    }

    trt_handle_close(handle);
    free(timer);
    return ERR_OK;
}

static int timer_active_obj(timer_t *timer)
{
    return timer != 0 && timer->active;
}

int trt_timer_active(trt_handle_t handle)
{
    timer_t *timer;

    if (timer_lookup(handle, TRT_RIGHT_READ, &timer) != ERR_OK)
    {
        return 0;
    }

    return timer_active_obj(timer);
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
