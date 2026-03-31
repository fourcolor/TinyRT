#include <stddef.h>
#include <stdint.h>
#include "critical.h"
#include "list.h"

#define HEAP_ALIGN 8u

typedef struct heap_block
{
    size_t size;
    int free;
    list_node_t node;
} heap_block_t;

extern char __heap_start[];
extern char __heap_end[];

static list_head_t heap_blocks;
static int heap_initialized;
static uintptr_t heap_start_addr;
static uintptr_t heap_end_addr;

static inline uintptr_t align_up(uintptr_t value, uintptr_t align)
{
    return (value + align - 1u) & ~(align - 1u);
}

static inline uintptr_t align_down(uintptr_t value, uintptr_t align)
{
    return value & ~(align - 1u);
}

static inline size_t align_size(size_t size)
{
    return (size + HEAP_ALIGN - 1u) & ~(HEAP_ALIGN - 1u);
}

static void heap_init_once(void)
{
    uintptr_t start;
    uintptr_t end;
    heap_block_t *first;

    if (heap_initialized)
    {
        return;
    }

    INIT_LIST_HEAD(&heap_blocks);

    start = align_up((uintptr_t)__heap_start, HEAP_ALIGN);
    end = align_down((uintptr_t)__heap_end, HEAP_ALIGN);
    heap_start_addr = start;
    heap_end_addr = end;

    if (end <= start + sizeof(heap_block_t))
    {
        heap_initialized = 1;
        return;
    }

    first = (heap_block_t *)start;
    first->size = end - start - sizeof(heap_block_t);
    first->free = 1;
    INIT_LIST_HEAD(&first->node);
    list_add_tail(&first->node, &heap_blocks);

    heap_initialized = 1;
}

static void split_block(heap_block_t *block, size_t size)
{
    uintptr_t new_addr;
    heap_block_t *new_block;

    if (block->size < size + sizeof(heap_block_t) + HEAP_ALIGN)
    {
        return;
    }

    new_addr = (uintptr_t)(block + 1) + size;
    new_block = (heap_block_t *)new_addr;
    new_block->size = block->size - size - sizeof(heap_block_t);
    new_block->free = 1;
    INIT_LIST_HEAD(&new_block->node);
    list_add(&new_block->node, &block->node);

    block->size = size;
}

static int blocks_are_adjacent(heap_block_t *left, heap_block_t *right)
{
    return (uintptr_t)(left + 1) + left->size == (uintptr_t)right;
}

static void merge_with_next(heap_block_t *block)
{
    heap_block_t *next;

    if (block->node.next == &heap_blocks)
    {
        return;
    }

    next = list_entry(block->node.next, heap_block_t, node);
    if (!next->free || !blocks_are_adjacent(block, next))
    {
        return;
    }

    block->size += sizeof(heap_block_t) + next->size;
    list_del(&next->node);
}

void *malloc(size_t size)
{
    uint32_t state;
    list_head_t *pos;

    if (size == 0)
    {
        return 0;
    }

    state = critical_enter();
    heap_init_once();
    size = align_size(size);

    list_for_each(pos, &heap_blocks)
    {
        heap_block_t *block = list_entry(pos, heap_block_t, node);

        if (block->free && block->size >= size)
        {
            split_block(block, size);
            block->free = 0;
            critical_exit(state);
            return (void *)(block + 1);
        }
    }

    critical_exit(state);
    return 0;
}

void free(void *ptr)
{
    uint32_t state;
    heap_block_t *block;

    if (ptr == 0)
    {
        return;
    }

    state = critical_enter();
    heap_init_once();

    block = ((heap_block_t *)ptr) - 1;
    block->free = 1;

    merge_with_next(block);
    if (block->node.prev != &heap_blocks)
    {
        heap_block_t *prev = list_entry(block->node.prev, heap_block_t, node);

        if (prev->free && blocks_are_adjacent(prev, block))
        {
            merge_with_next(prev);
        }
    }

    critical_exit(state);
}

void *calloc(size_t count, size_t size)
{
    size_t total;
    unsigned char *mem;

    if (count != 0 && size > ((size_t)-1) / count)
    {
        return 0;
    }

    total = count * size;
    mem = malloc(total);
    if (mem == 0)
    {
        return 0;
    }

    for (size_t i = 0; i < total; i++)
    {
        mem[i] = 0;
    }

    return mem;
}

void *realloc(void *ptr, size_t size)
{
    heap_block_t *block;
    void *new_ptr;
    size_t copy_size;
    unsigned char *dst;
    unsigned char *src;

    if (ptr == 0)
    {
        return malloc(size);
    }

    if (size == 0)
    {
        free(ptr);
        return 0;
    }

    block = ((heap_block_t *)ptr) - 1;
    size = align_size(size);
    if (block->size >= size)
    {
        uint32_t state = critical_enter();

        split_block(block, size);
        critical_exit(state);
        return ptr;
    }

    new_ptr = malloc(size);
    if (new_ptr == 0)
    {
        return 0;
    }

    copy_size = block->size < size ? block->size : size;
    dst = (unsigned char *)new_ptr;
    src = (unsigned char *)ptr;
    for (size_t i = 0; i < copy_size; i++)
    {
        dst[i] = src[i];
    }

    free(ptr);
    return new_ptr;
}

size_t heap_total_size(void)
{
    uint32_t state;
    size_t total;

    state = critical_enter();
    heap_init_once();
    total = heap_end_addr > heap_start_addr ? heap_end_addr - heap_start_addr - sizeof(heap_block_t)
                                            : 0;
    critical_exit(state);

    return total;
}

size_t heap_free_size(void)
{
    uint32_t state;
    size_t total = 0;
    list_head_t *pos;

    state = critical_enter();
    heap_init_once();

    list_for_each(pos, &heap_blocks)
    {
        heap_block_t *block = list_entry(pos, heap_block_t, node);

        if (block->free)
        {
            total += block->size;
        }
    }

    critical_exit(state);
    return total;
}
