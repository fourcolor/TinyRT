#pragma once
#include "error.h"
#include "wait_queue.h"
#include <stddef.h>

typedef struct
{
    unsigned char *buf;
    size_t cap;
    size_t qlen;
    volatile size_t head;
    volatile size_t tail;
    trt_wait_q_t readers;
    trt_wait_q_t writers;
} trt_msg_q_t;

trt_msg_q_t *trt_msg_q_init(size_t cap, size_t qlen);
err_t trt_msg_q_destroy(trt_msg_q_t *q);
err_t trt_msg_q_send(trt_msg_q_t *q, void *data, size_t size, trt_time_t timeout);
err_t trt_msg_q_send_front(trt_msg_q_t *q, void *data, size_t size, trt_time_t timeout);
err_t trt_msg_q_recv(trt_msg_q_t *q, void *buf, trt_time_t timeout);
err_t trt_msg_q_peek(trt_msg_q_t *q, void *buf, trt_time_t timeout);
err_t trt_msg_q_send_from_isr(trt_msg_q_t *q, void *data, size_t size);
err_t trt_msg_q_recv_from_isr(trt_msg_q_t *q, void *buf);
err_t trt_msg_q_peek_from_isr(trt_msg_q_t *q, void *buf);
int trt_msg_q_is_full(trt_msg_q_t *q);
int trt_msg_q_is_empty(trt_msg_q_t *q);
size_t trt_msg_q_count(trt_msg_q_t *q);
