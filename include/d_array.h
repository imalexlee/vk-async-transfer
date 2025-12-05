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

b8 d_array_reserve(d_array* array, u32 new_capacity);

b8 d_array_resize(d_array* array, u32 new_count);

void d_array_set_value(d_array* array, u32 index, const void* value);

void d_array_get_value(d_array* array, u32 index, void* value);
