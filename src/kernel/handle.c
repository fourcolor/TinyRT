#include "handle_private.h"
#include "critical.h"

#define HANDLE_INDEX_BITS 16u
#define HANDLE_INDEX_MASK ((1u << HANDLE_INDEX_BITS) - 1u)

typedef struct
{
    void *object;
    trt_obj_type_t type;
    uint32_t rights;
    uint16_t generation;
    uint8_t used;
} handle_slot_t;

static handle_slot_t handle_table[RTOS_HANDLE_MAX];
static int handle_initialized;

static inline uint16_t handle_index(trt_handle_t handle)
{
    return (uint16_t)(handle & HANDLE_INDEX_MASK);
}

static inline uint16_t handle_generation(trt_handle_t handle)
{
    return (uint16_t)(handle >> HANDLE_INDEX_BITS);
}

static inline trt_handle_t handle_make(uint16_t index, uint16_t generation)
{
    return ((trt_handle_t)generation << HANDLE_INDEX_BITS) | index;
}

static void handle_table_init_locked(void)
{
    uint32_t i;

    if (handle_initialized)
    {
        return;
    }

    for (i = 0; i < RTOS_HANDLE_MAX; i++)
    {
        handle_table[i].object = 0;
        handle_table[i].type = TRT_OBJ_ANY;
        handle_table[i].rights = 0;
        handle_table[i].generation = 1;
        handle_table[i].used = 0;
    }

    handle_initialized = 1;
}

static err_t handle_get_slot_locked(trt_handle_t handle, handle_slot_t **out)
{
    uint16_t index;
    handle_slot_t *slot;

    if (handle == TRT_HANDLE_INVALID || out == 0)
    {
        return ERR_INVAL;
    }

    index = handle_index(handle);
    if (index >= RTOS_HANDLE_MAX)
    {
        return ERR_INVAL;
    }

    slot = &handle_table[index];
    if (!slot->used || slot->generation != handle_generation(handle))
    {
        return ERR_INVAL;
    }

    *out = slot;
    return ERR_OK;
}

err_t trt_handle_alloc(void *object, trt_obj_type_t type, uint32_t rights, trt_handle_t *out)
{
    uint32_t i;
    critical_state_t state;

    if (object == 0 || out == 0 || type == TRT_OBJ_ANY)
    {
        return ERR_INVAL;
    }

    state = critical_enter();
    handle_table_init_locked();

    for (i = 0; i < RTOS_HANDLE_MAX; i++)
    {
        handle_slot_t *slot = &handle_table[i];

        if (slot->used)
        {
            continue;
        }

        slot->object = object;
        slot->type = type;
        slot->rights = rights;
        slot->used = 1;
        *out = handle_make((uint16_t)i, slot->generation);
        critical_exit(state);
        return ERR_OK;
    }

    critical_exit(state);
    return ERR_NO_MEM;
}

err_t trt_handle_lookup(trt_handle_t handle, trt_obj_type_t type, uint32_t rights, void **out)
{
    handle_slot_t *slot;
    critical_state_t state;
    err_t result;

    if (out == 0)
    {
        return ERR_INVAL;
    }

    state = critical_enter();
    handle_table_init_locked();

    result = handle_get_slot_locked(handle, &slot);
    if (result != ERR_OK)
    {
        critical_exit(state);
        return result;
    }

    if (type != TRT_OBJ_ANY && slot->type != type)
    {
        critical_exit(state);
        return ERR_INVAL;
    }

    if ((slot->rights & rights) != rights)
    {
        critical_exit(state);
        return ERR_PERM;
    }

    *out = slot->object;
    critical_exit(state);
    return ERR_OK;
}

err_t trt_handle_close(trt_handle_t handle)
{
    handle_slot_t *slot;
    critical_state_t state;
    err_t result;

    state = critical_enter();
    handle_table_init_locked();

    result = handle_get_slot_locked(handle, &slot);
    if (result != ERR_OK)
    {
        critical_exit(state);
        return result;
    }

    slot->object = 0;
    slot->type = TRT_OBJ_ANY;
    slot->rights = 0;
    slot->used = 0;
    slot->generation++;
    if (slot->generation == 0)
    {
        slot->generation = 1;
    }

    critical_exit(state);
    return ERR_OK;
}

int trt_handle_valid(trt_handle_t handle)
{
    void *object;

    return trt_handle_lookup(handle, TRT_OBJ_ANY, 0, &object) == ERR_OK;
}
