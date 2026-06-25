#include <stdint.h>

#include "board.h"
#include "hal.h"
#include "logger.h"
#include "task.h"
#include "timer.h"

static volatile uint32_t timer_a_hits;
static volatile uint32_t timer_b_hits;
static volatile uint32_t timer_c_hits;
static volatile uint32_t timer_d_hits;

static void timer_count_callback(void *arg)
{
    volatile uint32_t *counter = (volatile uint32_t *)arg;

    (*counter)++;
}

static void led_task(void *arg)
{
    static int started;

    UNUSED(arg);
    gpio_write(BOARD_LED_PIN, 1);
    if (!started)
    {
        started = 1;
        LOG_INFO("led task start\n");
    }

    for (;;)
    {
        gpio_write(BOARD_LED_PIN, 1);
        task_sleep(TRT_MS(500));
        gpio_write(BOARD_LED_PIN, 0);
        task_sleep(TRT_MS(500));
    }
}

static void log_task1(void *arg)
{
    static int started;

    UNUSED(arg);
    if (!started)
    {
        started = 1;
        LOG_INFO("log task1 start\n");
    }

    for (;;)
    {
        uint64_t count = timer_cycles();
        LOG_INFO("A: count_hi=%lu count_lo=%lu soft_tick=%lu\n", (uint32_t)(count >> 32),
                 (uint32_t)count, timer_ticks());
        task_sleep(TRT_MS(1000));
    }
}

static void log_task2(void *arg)
{
    static int started;

    UNUSED(arg);
    if (!started)
    {
        started = 1;
        LOG_INFO("log task2 start\n");
    }

    for (;;)
    {
        uint64_t count = timer_cycles();
        LOG_INFO("B: count_hi=%lu count_lo=%lu soft_tick=%lu\n", (uint32_t)(count >> 32),
                 (uint32_t)count, timer_ticks());
        task_sleep(TRT_MS(1000));
    }
}

static void timer_task1(void *arg)
{
    static trt_handle_t timer_a;
    static trt_handle_t timer_b;

    UNUSED(arg);
    LOG_INFO("timer task1 start\n");

    timer_a = trt_timer_create(timer_count_callback, (void *)&timer_a_hits);
    timer_b = trt_timer_create(timer_count_callback, (void *)&timer_b_hits);
    trt_timer_start(timer_a, TRT_MS(250), TRT_MS(250));
    trt_timer_start(timer_b, TRT_MS(700), TRT_MS(700));

    for (;;)
    {
        LOG_INFO("T1: tick=%lu a=%lu b=%lu\n", timer_ticks(), timer_a_hits, timer_b_hits);
        task_sleep(TRT_MS(1000));
    }
}

static void timer_task2(void *arg)
{
    static trt_handle_t timer_c;
    static trt_handle_t timer_d;

    UNUSED(arg);
    LOG_INFO("timer task2 start\n");

    timer_c = trt_timer_create(timer_count_callback, (void *)&timer_c_hits);
    timer_d = trt_timer_create(timer_count_callback, (void *)&timer_d_hits);
    trt_timer_start(timer_c, TRT_MS(333), TRT_MS(333));
    trt_timer_start(timer_d, TRT_MS(1200), TRT_MS(1200));

    for (;;)
    {
        LOG_INFO("T2: tick=%lu c=%lu d=%lu active=%d/%d\n", timer_ticks(), timer_c_hits,
                 timer_d_hits, trt_timer_active(timer_c), trt_timer_active(timer_d));
        task_sleep(TRT_MS(1000));
    }
}

void app_main(void)
{
    task_create("led_task", led_task, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("log_task1", log_task1, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("log_task2", log_task2, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("timer_task1", timer_task1, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("timer_task2", timer_task2, 0, RTOS_TASK_STACK_SIZE, 1);
}
