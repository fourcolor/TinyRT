#include "board.h"
#include "hal.h"
#include "logger.h"
#include "task.h"
#include "timer.h"

static void led_task(void *arg)
{
    UNUSED(arg);

    LOG_INFO("main demo start\n");

    for (;;)
    {
        gpio_write(BOARD_LED_PIN, 1);
        task_sleep(TRT_MS(500));
        gpio_write(BOARD_LED_PIN, 0);
        task_sleep(TRT_MS(500));
    }
}

void app_main(void)
{
    gpio_output(BOARD_LED_PIN);

    task_create("main_led", led_task, 0, RTOS_TASK_STACK_SIZE, 1);
}
