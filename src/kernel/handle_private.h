#pragma once

#include <stdint.h>
#include "error.h"
#include "handle.h"
#include "rtos_config.h"

#ifndef RTOS_HANDLE_MAX
#define RTOS_HANDLE_MAX 64
#endif

#if RTOS_HANDLE_MAX > 65536
#error "RTOS_HANDLE_MAX must fit in the 16-bit handle index field"
#endif

typedef enum
{
    TRT_OBJ_ANY = 0,
    TRT_OBJ_TASK,
    TRT_OBJ_SEM,
    TRT_OBJ_MUTEX,
    TRT_OBJ_MSG_Q,
    TRT_OBJ_TIMER,
} trt_obj_type_t;

enum
{
    TRT_RIGHT_READ = 1u << 0,
    TRT_RIGHT_WRITE = 1u << 1,
    TRT_RIGHT_WAIT = 1u << 2,
    TRT_RIGHT_POST = 1u << 3,
    TRT_RIGHT_DESTROY = 1u << 4,
    TRT_RIGHT_ALL = 0xffffffffu,
};

err_t trt_handle_alloc(void *object, trt_obj_type_t type, uint32_t rights, trt_handle_t *out);
err_t trt_handle_lookup(trt_handle_t handle, trt_obj_type_t type, uint32_t rights, void **out);
err_t trt_handle_close(trt_handle_t handle);
