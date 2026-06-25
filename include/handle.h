#pragma once

#include <stdint.h>

typedef uint32_t trt_handle_t;

#define TRT_HANDLE_INVALID ((trt_handle_t)0)

int trt_handle_valid(trt_handle_t handle);
