#include "queue.h"

#include <string.h>

static b8 d_queue_resize(d_queue* queue, u32 new_capacity) {

    void* temp = malloc(queue->element_size * new_capacity);
    if (!temp) {
        return false;
    }

    //  copy old memory to current from front to back
    if (queue->memory) {
        for (u32 i = 0; i < queue->count; ++i) {
            u32 old_element_pos = queue->front * queue->element_size;
            u32 new_element_pos = i * queue->element_size;

            memcpy(&temp[new_element_pos], &queue->memory[old_element_pos], queue->element_size);

            queue->front = (queue->front + 1) % queue->capacity;
        }

        free(queue->memory);
    }

    queue->memory   = temp;
    queue->capacity = new_capacity;
    queue->front    = 0;
    queue->back     = queue->count > 0 ? queue->count - 1 : 0;

    return true;
}

b8 d_queue_create(d_queue* queue, u32 element_size) {
    if (!queue) {
        return false;
    }

    queue->element_size = element_size;

    if (!d_queue_resize(queue, 50)) {
        return false;
    }

    return true;
}

void d_queue_destroy(d_queue* queue) {
    if (!queue) {
        return;
    }

    if (queue->memory) {
        free(queue->memory);
    }

    memset(queue, 0, sizeof(d_queue));
}

b8 d_queue_push(d_queue* queue, const void* element) {
    if (!queue || !element) {
        return false;
    }

    if (queue->count == queue->capacity) {
        if (!d_queue_resize(queue, queue->capacity * 2)) {
            return false;
        }
    }

    queue->back = (queue->back + 1) % queue->capacity;
    queue->count++;

    memcpy(&queue->memory[queue->back * queue->element_size], element, queue->element_size);

    return true;
}

b8 d_queue_pop(d_queue* queue, void* element) {
    if (!queue || !element) {
        return false;
    }

    if (queue->count == 0) {
        return false;
    }

    memcpy(element, &queue->memory[queue->front * queue->element_size], queue->element_size);

    queue->front = (queue->front + 1) % queue->capacity;
    queue->count++;

    return true;
}