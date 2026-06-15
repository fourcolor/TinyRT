#include <stdint.h>

#include "error.h"
#include "logger.h"
#include "msg_queue.h"
#include "rtos_config.h"
#include "task.h"
#include "timer.h"

#define MSG_SRC_TASK 1u
#define MSG_SRC_ISR_PRODUCER 2u
#define MSG_SRC_ISR_CONSUMER 3u

typedef struct
{
    uint32_t source;
    uint32_t seq;
    uint32_t tick;
} test_msg_t;

static trt_msg_q_t *task_q;
static trt_msg_q_t *isr_producer_q;
static trt_msg_q_t *isr_consumer_q;
static timer_t isr_producer_timer;
static timer_t isr_consumer_timer;

static volatile uint32_t task_sent;
static volatile uint32_t task_received;
static volatile uint32_t task_errors;
static volatile uint32_t isr_sent;
static volatile uint32_t isr_send_busy;
static volatile uint32_t isr_received_by_task;
static volatile uint32_t isr_consumer_fed;
static volatile uint32_t isr_consumed;
static volatile uint32_t isr_consumer_empty;
static volatile uint32_t isr_errors;

static int msg_matches(test_msg_t *msg, uint32_t source, uint32_t seq)
{
    return msg->source == source && msg->seq == seq;
}

static void log_result(const char *name, int ok, int result)
{
    LOG_INFO("MSG_Q %s %s result=%d tick=%lu\n", name, ok ? "ok" : "fail", result, timer_ticks());
}

static void isr_producer_cb(void *arg)
{
    static uint32_t seq;
    test_msg_t msg;
    err_t result;

    (void)arg;

    msg.source = MSG_SRC_ISR_PRODUCER;
    msg.seq = seq;
    msg.tick = timer_ticks();

    result = trt_msg_q_send_from_isr(isr_producer_q, &msg, sizeof(msg));
    if (result == ERR_OK)
    {
        seq++;
        isr_sent++;
    }
    else if (result == ERR_BUSY)
    {
        isr_send_busy++;
    }
    else
    {
        isr_errors++;
    }
}

static void isr_consumer_cb(void *arg)
{
    static uint32_t expected_seq;
    test_msg_t peeked;
    test_msg_t received;
    err_t result;

    (void)arg;

    result = trt_msg_q_peek_from_isr(isr_consumer_q, &peeked);
    if (result == ERR_BUSY)
    {
        isr_consumer_empty++;
        return;
    }
    if (result != ERR_OK)
    {
        isr_errors++;
        return;
    }

    result = trt_msg_q_recv_from_isr(isr_consumer_q, &received);
    if (result != ERR_OK)
    {
        isr_errors++;
        return;
    }

    if (!msg_matches(&peeked, MSG_SRC_ISR_CONSUMER, expected_seq) ||
        !msg_matches(&received, MSG_SRC_ISR_CONSUMER, expected_seq))
    {
        isr_errors++;
        expected_seq = received.seq + 1u;
    }
    else
    {
        expected_seq++;
    }

    isr_consumed++;
}

static void task_producer(void *arg)
{
    uint32_t seq = 0;

    (void)arg;

    for (;;)
    {
        test_msg_t msg;
        err_t result;

        msg.source = MSG_SRC_TASK;
        msg.seq = seq;
        msg.tick = timer_ticks();

        result = trt_msg_q_send(task_q, &msg, sizeof(msg), TRT_MS(1000));
        if (result == ERR_OK)
        {
            seq++;
            task_sent++;
        }
        else
        {
            task_errors++;
            LOG_INFO("MSG_Q task producer error result=%d seq=%lu tick=%lu\n", result, seq,
                     timer_ticks());
        }

        task_sleep(TRT_MS(20));
    }
}

static void task_consumer(void *arg)
{
    uint32_t expected_seq = 0;

    (void)arg;

    for (;;)
    {
        test_msg_t msg;
        err_t result;

        result = trt_msg_q_recv(task_q, &msg, TRT_MS(1000));
        if (result == ERR_OK)
        {
            if (!msg_matches(&msg, MSG_SRC_TASK, expected_seq))
            {
                task_errors++;
                LOG_INFO("MSG_Q task consumer mismatch got=%lu expect=%lu tick=%lu\n", msg.seq,
                         expected_seq, timer_ticks());
                expected_seq = msg.seq + 1u;
            }
            else
            {
                expected_seq++;
            }
            task_received++;
        }
        else
        {
            task_errors++;
            LOG_INFO("MSG_Q task consumer error result=%d expect=%lu tick=%lu\n", result,
                     expected_seq, timer_ticks());
        }

        task_sleep(TRT_MS(35));
    }
}

static void isr_supervisor(void *arg)
{
    uint32_t expected_isr_seq = 0;
    uint32_t feed_seq = 0;
    uint64_t last_log_us = 0;

    (void)arg;

    timer_setup(&isr_producer_timer, isr_producer_cb, 0);
    timer_setup(&isr_consumer_timer, isr_consumer_cb, 0);
    timer_start(&isr_producer_timer, TRT_MS(50), TRT_MS(50));
    timer_start(&isr_consumer_timer, TRT_MS(70), TRT_MS(70));

    for (;;)
    {
        test_msg_t msg;
        err_t result;
        uint64_t now_us;

        result = trt_msg_q_recv(isr_producer_q, &msg, TRT_MS(100));
        if (result == ERR_OK)
        {
            if (!msg_matches(&msg, MSG_SRC_ISR_PRODUCER, expected_isr_seq))
            {
                isr_errors++;
                LOG_INFO("MSG_Q ISR producer mismatch got=%lu expect=%lu tick=%lu\n", msg.seq,
                         expected_isr_seq, timer_ticks());
                expected_isr_seq = msg.seq + 1u;
            }
            else
            {
                expected_isr_seq++;
            }
            isr_received_by_task++;
        }
        else if (result != ERR_TIMEOUT && result != ERR_BUSY)
        {
            isr_errors++;
            LOG_INFO("MSG_Q ISR producer recv error result=%d tick=%lu\n", result, timer_ticks());
        }

        msg.source = MSG_SRC_ISR_CONSUMER;
        msg.seq = feed_seq;
        msg.tick = timer_ticks();
        result = trt_msg_q_send(isr_consumer_q, &msg, sizeof(msg), TRT_MS(100));
        if (result == ERR_OK)
        {
            feed_seq++;
            isr_consumer_fed++;
        }
        else if (result != ERR_TIMEOUT && result != ERR_BUSY)
        {
            isr_errors++;
            LOG_INFO("MSG_Q ISR consumer feed error result=%d seq=%lu tick=%lu\n", result, feed_seq,
                     timer_ticks());
        }

        now_us = timer_us();
        if ((now_us - last_log_us) >= TRT_SEC(1).us)
        {
            last_log_us = now_us;
            LOG_INFO("MSG_Q loop task=%lu/%lu task_err=%lu isr_send=%lu busy=%lu isr_recv=%lu "
                     "isr_feed=%lu isr_consume=%lu empty=%lu isr_err=%lu tick=%lu\n",
                     task_sent, task_received, task_errors, isr_sent, isr_send_busy,
                     isr_received_by_task, isr_consumer_fed, isr_consumed, isr_consumer_empty,
                     isr_errors, timer_ticks());
        }
    }
}

void app_main(void)
{
    err_t result;
    test_msg_t msg;
    trt_msg_q_t *destroy_q;

    LOG_INFO("MSG_Q cyclic test start tick=%lu\n", timer_ticks());

    destroy_q = trt_msg_q_init(sizeof(test_msg_t), 1);
    result = destroy_q == 0 ? ERR_NO_MEM : trt_msg_q_destroy(destroy_q);
    log_result("destroy", result == ERR_OK, result);

    task_q = trt_msg_q_init(sizeof(test_msg_t), 3);
    isr_producer_q = trt_msg_q_init(sizeof(test_msg_t), 4);
    isr_consumer_q = trt_msg_q_init(sizeof(test_msg_t), 3);

    if (task_q == 0 || isr_producer_q == 0 || isr_consumer_q == 0)
    {
        LOG_INFO("MSG_Q init fail task_q=%p isr_prod=%p isr_cons=%p\n", task_q, isr_producer_q,
                 isr_consumer_q);
        return;
    }

    msg.source = MSG_SRC_TASK;
    msg.seq = 0;
    msg.tick = timer_ticks();
    result = trt_msg_q_send_from_isr(task_q, &msg, sizeof(msg));
    log_result("send_from_task", result == ERR_STATE, result);
    result = trt_msg_q_recv_from_isr(task_q, &msg);
    log_result("recv_from_task", result == ERR_STATE, result);
    result = trt_msg_q_peek_from_isr(task_q, &msg);
    log_result("peek_from_task", result == ERR_STATE, result);

    task_create("msgq_prod", task_producer, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("msgq_cons", task_consumer, 0, RTOS_TASK_STACK_SIZE, 1);
    task_create("msgq_isr", isr_supervisor, 0, RTOS_TASK_STACK_SIZE, 2);
}
