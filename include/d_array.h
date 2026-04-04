#pragma once

#include "common.h"

// fixed, dynamically allocated array
typedef struct d_array {
    void* memory;
    u32   element_size;
    // number of elements currently in the array
    u32 count;
    // number of elements the array has space for (not bytes)
    u32 capacity;
} d_array;

b8 d_array_create(d_array* array, u32 element_size, u32 capacity);

void d_array_destroy(d_array* array);

void* d_array_at(const d_array* array, u32 index);

b8 d_array_push(d_array* array, const void* element);

b8 d_array_pop(d_array* array, void* element);
