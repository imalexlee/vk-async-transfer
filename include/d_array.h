#pragma once

#include "common.h"

// dynamic array
typedef struct d_array {
    void* memory;
    u32   element_size;
    // number of elements currently in the array
    u32 count;
    // number of elements the array has space for (not bytes)
    u32 capacity;
} d_array;

b8 d_array_create(d_array* array, u32 element_size, u32 initial_capacity);

void d_array_destroy(d_array* array);

b8 d_array_resize(d_array* array, u32 new_count);

void* d_array_at(d_array* array, u32 index);

b8 d_array_push_back(d_array* array, const void* value);

b8 d_array_pop_back(d_array* array, void* value);
