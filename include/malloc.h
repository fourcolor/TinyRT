#pragma once

#include <stddef.h>

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t size);

size_t heap_total_size(void);
size_t heap_free_size(void);
