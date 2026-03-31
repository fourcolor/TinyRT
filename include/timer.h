#pragma once

#include <stdint.h>
#include "list.h"

typedef void (*timer_callback_t)(void *);

typedef struct
{
    uint64_t us;
} trt_time_t;

#define TRT_US(value) ((trt_time_t){.us = (uint64_t)(value)})
#define TRT_MS(value) ((trt_time_t){.us = (uint64_t)(value)*1000ull})
#define TRT_SEC(value) ((trt_time_t){.us = (uint64_t)(value)*1000000ull})

typedef struct timer_t
{
    list_node_t node;
    uint32_t deadline;
    uint32_t period;
    timer_callback_t callback;
    void *arg;
    int active;
} timer_t;

void timer_init(void);
void timer_start_tick(void);
uint32_t timer_ticks(void);
uint64_t timer_cycles(void);
uint64_t timer_us(void);
uint32_t timer_us_to_ticks(uint64_t us);
uint32_t timer_ms_to_ticks(uint64_t ms);
uint32_t timer_sec_to_ticks(uint64_t sec);
int timer_expired(uint32_t now, uint32_t deadline);

void timer_setup(timer_t *timer, timer_callback_t callback, void *arg);
void timer_start(timer_t *timer, uint32_t delay_ticks, uint32_t period_ticks);
void timer_stop(timer_t *timer);
int timer_active(timer_t *timer);
void timer_run_expired(void);
