#include "kernel.h"
#include "port.h"
#include "logger.h"
#include "task.h"
#include "sched.h"
#include "timer.h"

void __attribute__((weak)) app_main(void) {}

static void idle_task(void *arg)
{

    for (;;)
    {
        task_cleanup_deleted();
        arch_interrupt_enable();
        arch_wait_for_interrupt();
    }
}

static void kernel_init(void)
{
    task_t *idle;

    arch_interrupt_disable();
    task_init();

    idle = task_create("idle_task", idle_task, 0, RTOS_TASK_STACK_SIZE, 0);
    if (idle == 0)
    {
        LOG_ERROR("idle task create failed\n");
        for (;;)
            ;
    }
    sched_set_idle_task(idle);
}

void rtos_start(void)
{
    arch_interrupt_disable();
    LOG_DEBUG("rtos_start\n");

    scheduler.current_task = sched_pick_next();
    if (scheduler.current_task == 0)
    {
        LOG_ERROR("no runnable task\n");
        for (;;)
            ;
    }

    LOG_DEBUG("first task=%p sp=%p entry=%p\n", scheduler.current_task, scheduler.current_task->sp,
              arch_task_frame_entry(scheduler.current_task->sp));
    timer_init();
    timer_start_tick();
    LOG_DEBUG("start first task\n");
    task_start_first(scheduler.current_task->sp);
}

void kernel_main(void)
{
    kernel_init();
    LOG_INFO("kernel_main\n");

    app_main();
    arch_interrupt_disable();
    rtos_start();

    for (;;)
        ;
}
