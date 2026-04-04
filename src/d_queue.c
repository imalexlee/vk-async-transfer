#include "d_queue.h"

b8 d_queue_create(d_queue* queue, u32 element_size, u32 capacity) {
    assert(queue);

    memset(queue, 0, sizeof(d_queue));

    queue->element_size = element_size;
    queue->capacity     = capacity;
    queue->memory       = malloc(element_size * capacity);

    if (!queue->memory) {
        return false;
    }

    return true;
}

void d_queue_destroy(d_queue* queue) {
    assert(queue);

    if (queue->memory) {
        free(queue->memory);
    }

    memset(queue, 0, sizeof(d_queue));
}

b8 d_queue_push(d_queue* queue, const void* element) {
    assert(queue);
    assert(element);

    if (queue->count == queue->capacity) {
        return false;
    }

    u8* write_address = (u8*)queue->memory + queue->back * queue->element_size;
    memcpy(write_address, element, queue->element_size);

    queue->back++;
    queue->back = queue->back == queue->capacity ? 0 : queue->back;

    queue->count++;

    return true;
}

b8 d_queue_pop(d_queue* queue, void* element) {
    assert(queue);
    assert(element);

    if (queue->count == 0) {
        return false;
    }

    const u8* read_address = (u8*)queue->memory + queue->front * queue->element_size;
    memcpy(element, read_address, queue->element_size);

    queue->front++;
    queue->front = queue->front == queue->capacity ? 0 : queue->front;

    queue->count--;

    return true;
}