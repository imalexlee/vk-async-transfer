#pragma once

#include "common.h"

// dynamic queue
typedef struct d_queue {
    void* memory;
    u32   element_size;
    // number of elements currently in the queue
    u32 count;
    // number of elements the queue has space for (not bytes)
    u32 capacity;

    u32 back;
    u32 front;
} d_queue;

b8 d_queue_create(d_queue* queue, u32 element_size, u32 initial_capacity);

b8 d_queue_push(d_queue* queue, const void* element);

b8 d_queue_pop(d_queue* queue, void* element);

void d_queue_destroy(d_queue* queue);
