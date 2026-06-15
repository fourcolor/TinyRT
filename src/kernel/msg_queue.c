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

static size_t msg_q_next(trt_msg_q_t *mq, size_t index)
{
    return (index + 1u) % (mq->qlen + 1u);
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

int trt_msg_q_is_full(trt_msg_q_t *mq)
{
    if (mq == 0)
    {
        return 0;
    }

    return msg_q_next(mq, mq->head) == mq->tail;
}

int trt_msg_q_is_empty(trt_msg_q_t *mq)
{
    if (mq == 0)
    {
        return 1;
    }

    return mq->head == mq->tail;
}

size_t trt_msg_q_count(trt_msg_q_t *mq)
{
    size_t count;
    critical_state_t state;

    if (mq == 0)
    {
        return 0;
    }

    state = critical_enter();
    if (mq->head >= mq->tail)
    {
        count = mq->head - mq->tail;
    }
    else
    {
        count = (mq->qlen + 1u) - mq->tail + mq->head;
    }
    critical_exit(state);

    return count;
}

trt_msg_q_t *trt_msg_q_init(size_t cap, size_t qlen)
{
    trt_msg_q_t *mq;

    if (!cap || !qlen)
    {
        return 0;
    }

    mq = calloc(1, sizeof(*mq));
    if (mq == 0)
    {
        return 0;
    }

    mq->buf = calloc(qlen + 1u, cap);
    if (mq->buf == 0)
    {
        free(mq);
        return 0;
    }

    mq->cap = cap;
    mq->qlen = qlen;
    mq->head = 0;
    mq->tail = 0;
    mq->destroyed = 0;
    trt_wait_q_init(&mq->readers);
    trt_wait_q_init(&mq->writers);

    return mq;
}

err_t trt_msg_q_destroy(trt_msg_q_t *mq)
{
    critical_state_t state;

    if (mq == 0)
    {
        return ERR_INVAL;
    }

    state = critical_enter();
    if (mq->destroyed)
    {
        critical_exit(state);
        return ERR_STATE;
    }

    mq->destroyed = 1;
    trt_wait_q_wake_all_result_locked(&mq->readers, TASK_WAIT_DESTROYED);
    trt_wait_q_wake_all_result_locked(&mq->writers, TASK_WAIT_DESTROYED);
    critical_exit(state);

    free(mq->buf);
    free(mq);
    return ERR_OK;
}

err_t trt_msg_q_send(trt_msg_q_t *mq, void *data, size_t size, trt_time_t timeout)
{
    int result;
    critical_state_t state;

    if (mq == 0 || data == 0 || size == 0 || size > mq->cap)
    {
        return ERR_INVAL;
    }

    for (;;)
    {
        unsigned char *slot;

        state = critical_enter();

        if (mq->destroyed)
        {
            critical_exit(state);
            return ERR_DESTROYED;
        }

        if (!trt_msg_q_is_full(mq))
        {
            slot = mq->buf + (mq->head * mq->cap);
            msg_q_zero(slot, mq->cap);
            msg_q_copy(slot, data, size);
            mq->head = msg_q_next(mq, mq->head);
            trt_wait_q_wake_one_locked(&mq->readers);
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
                     ? trt_wait_q_block_locked(&mq->writers)
                     : trt_wait_q_block_timeout_locked(&mq->writers, timeout);
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

err_t trt_msg_q_send_front(trt_msg_q_t *mq, void *data, size_t size, trt_time_t timeout)
{
    int result;
    critical_state_t state;

    if (mq == 0 || data == 0 || size == 0 || size > mq->cap)
    {
        return ERR_INVAL;
    }

    for (;;)
    {
        unsigned char *slot;

        state = critical_enter();

        if (mq->destroyed)
        {
            critical_exit(state);
            return ERR_DESTROYED;
        }

        if (!trt_msg_q_is_full(mq))
        {
            mq->tail = mq->tail == 0 ? mq->qlen : mq->tail - 1u;
            slot = mq->buf + (mq->tail * mq->cap);
            msg_q_zero(slot, mq->cap);
            msg_q_copy(slot, data, size);
            trt_wait_q_wake_one_locked(&mq->readers);
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
                     ? trt_wait_q_block_locked(&mq->writers)
                     : trt_wait_q_block_timeout_locked(&mq->writers, timeout);
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

err_t trt_msg_q_recv(trt_msg_q_t *mq, void *buf, trt_time_t timeout)
{
    int result;
    critical_state_t state;

    if (mq == 0 || buf == 0)
    {
        return ERR_INVAL;
    }

    for (;;)
    {
        unsigned char *slot;

        state = critical_enter();

        if (mq->destroyed)
        {
            critical_exit(state);
            return ERR_DESTROYED;
        }

        if (!trt_msg_q_is_empty(mq))
        {
            slot = mq->buf + (mq->tail * mq->cap);
            msg_q_copy(buf, slot, mq->cap);
            mq->tail = msg_q_next(mq, mq->tail);
            trt_wait_q_wake_one_locked(&mq->writers);
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
                     ? trt_wait_q_block_locked(&mq->readers)
                     : trt_wait_q_block_timeout_locked(&mq->readers, timeout);
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

err_t trt_msg_q_peek(trt_msg_q_t *mq, void *buf, trt_time_t timeout)
{
    int result;
    critical_state_t state;

    if (mq == 0 || buf == 0)
    {
        return ERR_INVAL;
    }

    for (;;)
    {
        unsigned char *slot;

        state = critical_enter();

        if (mq->destroyed)
        {
            critical_exit(state);
            return ERR_DESTROYED;
        }

        if (!trt_msg_q_is_empty(mq))
        {
            slot = mq->buf + (mq->tail * mq->cap);
            msg_q_copy(buf, slot, mq->cap);
            trt_wait_q_wake_one_locked(&mq->readers);
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
                     ? trt_wait_q_block_locked(&mq->readers)
                     : trt_wait_q_block_timeout_locked(&mq->readers, timeout);
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

err_t trt_msg_q_send_from_isr(trt_msg_q_t *mq, void *data, size_t size)
{
    critical_state_t state;
    unsigned char *slot;

    if (!arch_in_isr())
    {
        return ERR_STATE;
    }

    state = critical_enter();

    if (mq == 0 || data == 0 || size == 0 || size > mq->cap)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if (mq->destroyed)
    {
        critical_exit(state);
        return ERR_DESTROYED;
    }

    if (trt_msg_q_is_full(mq))
    {
        critical_exit(state);
        return ERR_BUSY;
    }

    slot = mq->buf + (mq->head * mq->cap);
    msg_q_zero(slot, mq->cap);
    msg_q_copy(slot, data, size);
    mq->head = msg_q_next(mq, mq->head);
    trt_wait_q_wake_one_from_isr(&mq->readers);
    critical_exit(state);
    return ERR_OK;
}

err_t trt_msg_q_recv_from_isr(trt_msg_q_t *mq, void *buf)
{
    critical_state_t state;
    unsigned char *slot;

    if (!arch_in_isr())
    {
        return ERR_STATE;
    }

    state = critical_enter();

    if (mq == 0 || buf == 0)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if (mq->destroyed)
    {
        critical_exit(state);
        return ERR_DESTROYED;
    }

    if (trt_msg_q_is_empty(mq))
    {
        critical_exit(state);
        return ERR_BUSY;
    }

    slot = mq->buf + (mq->tail * mq->cap);
    msg_q_copy(buf, slot, mq->cap);
    mq->tail = msg_q_next(mq, mq->tail);
    trt_wait_q_wake_one_from_isr(&mq->writers);
    critical_exit(state);
    return ERR_OK;
}

err_t trt_msg_q_peek_from_isr(trt_msg_q_t *mq, void *buf)
{
    critical_state_t state;
    unsigned char *slot;

    if (!arch_in_isr())
    {
        return ERR_STATE;
    }

    state = critical_enter();

    if (mq == 0 || buf == 0)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if (mq->destroyed)
    {
        critical_exit(state);
        return ERR_DESTROYED;
    }

    if (trt_msg_q_is_empty(mq))
    {
        critical_exit(state);
        return ERR_BUSY;
    }

    slot = mq->buf + (mq->tail * mq->cap);
    msg_q_copy(buf, slot, mq->cap);
    trt_wait_q_wake_one_from_isr(&mq->readers);
    critical_exit(state);
    return ERR_OK;
}
