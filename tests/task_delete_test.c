#include "error.h"
#include "logger.h"
#include "malloc.h"
#include "rtos_config.h"
#include "task.h"
#include "timer.h"

static size_t free_before;

static void return_task(void *arg)
{
    UNUSED(arg);

    LOG_INFO("return task start tick=%lu\n", timer_ticks());
}

static void self_delete_task(void *arg)
{
    UNUSED(arg);

    LOG_INFO("self delete task start tick=%lu\n", timer_ticks());
    task_exit();
    LOG_ERROR("self delete task returned\n");
}

static void supervisor_task(void *arg)
{
    size_t free_after_create;
    size_t free_after_cleanup;

    UNUSED(arg);

    free_before = heap_free_size();
    task_create("return_task", return_task, 0, RTOS_TASK_STACK_SIZE, 2);
    task_create("self_delete", self_delete_task, 0, RTOS_TASK_STACK_SIZE, 2);
    free_after_create = heap_free_size();
    LOG_INFO("delete test heap before=%lu after_create=%lu\n", free_before, free_after_create);

    task_sleep(TRT_MS(100));
    free_after_cleanup = heap_free_size();
    LOG_INFO("delete test heap after_cleanup=%lu reclaimed=%lu\n", free_after_cleanup,
             free_after_cleanup - free_after_create);

    for (;;)
    {
        task_sleep(TRT_MS(1000));
    }
}

void app_main(void)
{
    task_create("delete_super", supervisor_task, 0, RTOS_TASK_STACK_SIZE, 1);
}
