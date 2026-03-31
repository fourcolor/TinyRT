#pragma once

typedef enum
{
    ERR_OK = 0,
    ERR_INVAL = -1,
    ERR_NO_MEM = -2,
    ERR_BUSY = -3,
    ERR_TIMEOUT = -4,
    ERR_LOCKED = -5,
    ERR_PERM = -6,
    ERR_STATE = -7,
} err_t;
