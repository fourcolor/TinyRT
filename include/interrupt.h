#pragma once

typedef struct intr_handler_t
{
    void (*fn)(void *arg);
    void *arg;
} intr_handler_t;

/* Register a handler for a board/CPU interrupt line.
 * The interrupt source routing and CPU line allocation remain board-specific. */
void intr_register(int cpu_line, const intr_handler_t *handler);
