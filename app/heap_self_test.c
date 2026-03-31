#include <stdint.h>
#include "logger.h"
#include "malloc.h"

static int heap_self_test(void)
{
    size_t before;
    size_t after;
    unsigned char *a;
    unsigned char *b;
    unsigned char *c;
    unsigned char *grown;

    before = heap_free_size();
    if (before == 0)
    {
        return -1;
    }

    a = malloc(24);
    b = malloc(128);
    c = calloc(8, 4);
    if (a == 0 || b == 0 || c == 0)
    {
        free(a);
        free(b);
        free(c);
        return -2;
    }

    if (((uintptr_t)a & 7u) != 0 || ((uintptr_t)b & 7u) != 0 || ((uintptr_t)c & 7u) != 0)
    {
        free(a);
        free(b);
        free(c);
        return -3;
    }

    for (size_t i = 0; i < 32; i++)
    {
        if (c[i] != 0)
        {
            free(a);
            free(b);
            free(c);
            return -4;
        }
    }

    for (size_t i = 0; i < 24; i++)
    {
        a[i] = (unsigned char)(0xa0u + i);
    }
    for (size_t i = 0; i < 128; i++)
    {
        b[i] = (unsigned char)(0x40u + i);
    }

    grown = realloc(a, 64);
    if (grown == 0)
    {
        free(a);
        free(b);
        free(c);
        return -5;
    }

    for (size_t i = 0; i < 24; i++)
    {
        if (grown[i] != (unsigned char)(0xa0u + i))
        {
            free(grown);
            free(b);
            free(c);
            return -6;
        }
    }

    free(b);
    free(grown);
    free(c);

    after = heap_free_size();
    if (after != before)
    {
        return -7;
    }

    return 0;
}

void app_main(void)
{
    int heap_test = heap_self_test();

    LOG_INFO("heap self test=%d total=%lu free=%lu\n", heap_test, heap_total_size(),
             heap_free_size());
}
