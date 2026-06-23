#pragma once
#include "error.h"
#include "handle.h"
#include "timer.h"
#include <stddef.h>

trt_handle_t trt_msg_q_create(size_t cap, size_t qlen);
err_t trt_msg_q_destroy(trt_handle_t q);
err_t trt_msg_q_send(trt_handle_t q, void *data, size_t size, trt_time_t timeout);
err_t trt_msg_q_send_front(trt_handle_t q, void *data, size_t size, trt_time_t timeout);
err_t trt_msg_q_recv(trt_handle_t q, void *buf, trt_time_t timeout);
err_t trt_msg_q_peek(trt_handle_t q, void *buf, trt_time_t timeout);
err_t trt_msg_q_send_from_isr(trt_handle_t q, void *data, size_t size);
err_t trt_msg_q_recv_from_isr(trt_handle_t q, void *buf);
err_t trt_msg_q_peek_from_isr(trt_handle_t q, void *buf);
int trt_msg_q_is_full(trt_handle_t q);
int trt_msg_q_is_empty(trt_handle_t q);
size_t trt_msg_q_count(trt_handle_t q);
