#include "msg_queue.h"
#include "critical.h"
#include "error.h"
#include "hal.h"
#include "malloc.h"
#include "sched.h"
#include "task.h"
#include "wait_queue.h"

static err_t msg_q_wait_result(void)
{
    if (scheduler.current_task->wait_result == TASK_WAIT_OBJECT)
    {
        return ERR_OK;
    }
    if (scheduler.current_task->wait_result == TASK_WAIT_TIMEOUT)
    {
        return ERR_TIMEOUT;
    }
    if (scheduler.current_task->wait_result == TASK_WAIT_DESTROYED)
    {
        return ERR_DESTROYED;
    }

    return ERR_STATE;
}

static size_t msg_q_next(trt_msg_q_t *q, size_t index)
{
    return (index + 1u) % (q->qlen + 1u);
}

static void msg_q_zero(unsigned char *dst, size_t size)
{
    size_t i;

    for (i = 0; i < size; i++)
    {
        dst[i] = 0;
    }
}

static void msg_q_copy(unsigned char *dst, const void *src, size_t size)
{
    const unsigned char *bytes = src;
    size_t i;

    for (i = 0; i < size; i++)
    {
        dst[i] = bytes[i];
    }
}

int trt_msg_q_is_full(trt_msg_q_t *q)
{
    if (q == 0)
    {
        return 0;
    }

    return msg_q_next(q, q->head) == q->tail;
}

int trt_msg_q_is_empty(trt_msg_q_t *q)
{
    if (q == 0)
    {
        return 1;
    }

    return q->head == q->tail;
}

size_t trt_msg_q_count(trt_msg_q_t *q)
{
    size_t count;
    critical_state_t state;

    if (q == 0)
    {
        return 0;
    }

    state = critical_enter();
    if (q->head >= q->tail)
    {
        count = q->head - q->tail;
    }
    else
    {
        count = (q->qlen + 1u) - q->tail + q->head;
    }
    critical_exit(state);

    return count;
}

trt_msg_q_t *trt_msg_q_init(size_t cap, size_t qlen)
{
    trt_msg_q_t *q;

    if (!cap || !qlen)
    {
        return 0;
    }

    q = calloc(1, sizeof(*q));
    if (q == 0)
    {
        return 0;
    }

    q->buf = calloc(qlen + 1u, cap);
    if (q->buf == 0)
    {
        free(q);
        return 0;
    }

    q->cap = cap;
    q->qlen = qlen;
    q->head = 0;
    q->tail = 0;
    q->destroyed = 0;
    trt_wait_q_init(&q->readers);
    trt_wait_q_init(&q->writers);

    return q;
}

err_t trt_msg_q_destroy(trt_msg_q_t *q)
{
    critical_state_t state;

    if (q == 0)
    {
        return ERR_INVAL;
    }

    state = critical_enter();
    if (q->destroyed)
    {
        critical_exit(state);
        return ERR_STATE;
    }

    q->destroyed = 1;
    trt_wait_q_wake_all_result_locked(&q->readers, TASK_WAIT_DESTROYED);
    trt_wait_q_wake_all_result_locked(&q->writers, TASK_WAIT_DESTROYED);
    critical_exit(state);

    free(q->buf);
    free(q);
    return ERR_OK;
}

err_t trt_msg_q_send(trt_msg_q_t *q, void *data, size_t size, trt_time_t timeout)
{
    int result;
    critical_state_t state;

    if (q == 0 || data == 0 || size == 0 || size > q->cap)
    {
        return ERR_INVAL;
    }

    for (;;)
    {
        unsigned char *slot;

        state = critical_enter();

        if (q->destroyed)
        {
            critical_exit(state);
            return ERR_DESTROYED;
        }

        if (!trt_msg_q_is_full(q))
        {
            slot = q->buf + (q->head * q->cap);
            msg_q_zero(slot, q->cap);
            msg_q_copy(slot, data, size);
            q->head = msg_q_next(q, q->head);
            trt_wait_q_wake_one_locked(&q->readers);
            critical_exit(state);
            return ERR_OK;
        }

        if (timeout.us == 0)
        {
            critical_exit(state);
            return ERR_BUSY;
        }

        if (sched_is_locked())
        {
            sched_set_pending();
            critical_exit(state);
            return ERR_LOCKED;
        }

        result = timeout.us == TRT_TIME_FOREVER_US
                     ? trt_wait_q_block_locked(&q->writers)
                     : trt_wait_q_block_timeout_locked(&q->writers, timeout);
        critical_exit(state);

        if (result != ERR_OK)
        {
            return result;
        }

        task_yield();
        result = msg_q_wait_result();
        if (result != ERR_OK)
        {
            return result;
        }
    }
}

err_t trt_msg_q_send_front(trt_msg_q_t *q, void *data, size_t size, trt_time_t timeout)
{
    int result;
    critical_state_t state;

    if (q == 0 || data == 0 || size == 0 || size > q->cap)
    {
        return ERR_INVAL;
    }

    for (;;)
    {
        unsigned char *slot;

        state = critical_enter();

        if (q->destroyed)
        {
            critical_exit(state);
            return ERR_DESTROYED;
        }

        if (!trt_msg_q_is_full(q))
        {
            q->tail = q->tail == 0 ? q->qlen : q->tail - 1u;
            slot = q->buf + (q->tail * q->cap);
            msg_q_zero(slot, q->cap);
            msg_q_copy(slot, data, size);
            trt_wait_q_wake_one_locked(&q->readers);
            critical_exit(state);
            return ERR_OK;
        }

        if (timeout.us == 0)
        {
            critical_exit(state);
            return ERR_BUSY;
        }

        if (sched_is_locked())
        {
            sched_set_pending();
            critical_exit(state);
            return ERR_LOCKED;
        }

        result = timeout.us == TRT_TIME_FOREVER_US
                     ? trt_wait_q_block_locked(&q->writers)
                     : trt_wait_q_block_timeout_locked(&q->writers, timeout);
        critical_exit(state);

        if (result != ERR_OK)
        {
            return result;
        }

        task_yield();
        result = msg_q_wait_result();
        if (result != ERR_OK)
        {
            return result;
        }
    }
}

err_t trt_msg_q_recv(trt_msg_q_t *q, void *buf, trt_time_t timeout)
{
    int result;
    critical_state_t state;

    if (q == 0 || buf == 0)
    {
        return ERR_INVAL;
    }

    for (;;)
    {
        unsigned char *slot;

        state = critical_enter();

        if (q->destroyed)
        {
            critical_exit(state);
            return ERR_DESTROYED;
        }

        if (!trt_msg_q_is_empty(q))
        {
            slot = q->buf + (q->tail * q->cap);
            msg_q_copy(buf, slot, q->cap);
            q->tail = msg_q_next(q, q->tail);
            trt_wait_q_wake_one_locked(&q->writers);
            critical_exit(state);
            return ERR_OK;
        }

        if (timeout.us == 0)
        {
            critical_exit(state);
            return ERR_BUSY;
        }

        if (sched_is_locked())
        {
            sched_set_pending();
            critical_exit(state);
            return ERR_LOCKED;
        }

        result = timeout.us == TRT_TIME_FOREVER_US
                     ? trt_wait_q_block_locked(&q->readers)
                     : trt_wait_q_block_timeout_locked(&q->readers, timeout);
        critical_exit(state);

        if (result != ERR_OK)
        {
            return result;
        }

        task_yield();
        result = msg_q_wait_result();
        if (result != ERR_OK)
        {
            return result;
        }
    }
}

err_t trt_msg_q_peek(trt_msg_q_t *q, void *buf, trt_time_t timeout)
{
    int result;
    critical_state_t state;

    if (q == 0 || buf == 0)
    {
        return ERR_INVAL;
    }

    for (;;)
    {
        unsigned char *slot;

        state = critical_enter();

        if (q->destroyed)
        {
            critical_exit(state);
            return ERR_DESTROYED;
        }

        if (!trt_msg_q_is_empty(q))
        {
            slot = q->buf + (q->tail * q->cap);
            msg_q_copy(buf, slot, q->cap);
            trt_wait_q_wake_one_locked(&q->readers);
            critical_exit(state);
            return ERR_OK;
        }

        if (timeout.us == 0)
        {
            critical_exit(state);
            return ERR_BUSY;
        }

        if (sched_is_locked())
        {
            sched_set_pending();
            critical_exit(state);
            return ERR_LOCKED;
        }

        result = timeout.us == TRT_TIME_FOREVER_US
                     ? trt_wait_q_block_locked(&q->readers)
                     : trt_wait_q_block_timeout_locked(&q->readers, timeout);
        critical_exit(state);

        if (result != ERR_OK)
        {
            return result;
        }

        task_yield();
        result = msg_q_wait_result();
        if (result != ERR_OK)
        {
            return result;
        }
    }
}

err_t trt_msg_q_send_from_isr(trt_msg_q_t *q, void *data, size_t size)
{
    critical_state_t state;
    unsigned char *slot;

    if (!arch_in_isr())
    {
        return ERR_STATE;
    }

    state = critical_enter();

    if (q == 0 || data == 0 || size == 0 || size > q->cap)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if (q->destroyed)
    {
        critical_exit(state);
        return ERR_DESTROYED;
    }

    if (trt_msg_q_is_full(q))
    {
        critical_exit(state);
        return ERR_BUSY;
    }

    slot = q->buf + (q->head * q->cap);
    msg_q_zero(slot, q->cap);
    msg_q_copy(slot, data, size);
    q->head = msg_q_next(q, q->head);
    trt_wait_q_wake_one_from_isr(&q->readers);
    critical_exit(state);
    return ERR_OK;
}

err_t trt_msg_q_recv_from_isr(trt_msg_q_t *q, void *buf)
{
    critical_state_t state;
    unsigned char *slot;

    if (!arch_in_isr())
    {
        return ERR_STATE;
    }

    state = critical_enter();

    if (q == 0 || buf == 0)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if (q->destroyed)
    {
        critical_exit(state);
        return ERR_DESTROYED;
    }

    if (trt_msg_q_is_empty(q))
    {
        critical_exit(state);
        return ERR_BUSY;
    }

    slot = q->buf + (q->tail * q->cap);
    msg_q_copy(buf, slot, q->cap);
    q->tail = msg_q_next(q, q->tail);
    trt_wait_q_wake_one_from_isr(&q->writers);
    critical_exit(state);
    return ERR_OK;
}

err_t trt_msg_q_peek_from_isr(trt_msg_q_t *q, void *buf)
{
    critical_state_t state;
    unsigned char *slot;

    if (!arch_in_isr())
    {
        return ERR_STATE;
    }

    state = critical_enter();

    if (q == 0 || buf == 0)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if (q->destroyed)
    {
        critical_exit(state);
        return ERR_DESTROYED;
    }

    if (trt_msg_q_is_empty(q))
    {
        critical_exit(state);
        return ERR_BUSY;
    }

    slot = q->buf + (q->tail * q->cap);
    msg_q_copy(buf, slot, q->cap);
    trt_wait_q_wake_one_from_isr(&q->readers);
    critical_exit(state);
    return ERR_OK;
}
