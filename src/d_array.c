#include "d_array.h"

b8 d_array_create(d_array* array, u32 element_size, u32 capacity) {
    memset(array, 0, sizeof(d_array));

    array->element_size = element_size;
    array->capacity     = capacity;
    array->memory       = malloc(element_size * capacity);

    if (!array->memory) {
        return false;
    }

    return true;
}

void d_array_destroy(d_array* array) {
    assert(array);

    if (array->memory) {
        free(array->memory);
    }

    memset(array, 0, sizeof(d_array));
}

void* d_array_at(const d_array* array, u32 index) {
    assert(array);
    assert(index < array->count);

    return (u8*)array->memory + array->element_size * index;
}

b8 d_array_push(d_array* array, const void* element) {
    assert(array);
    assert(element);

    if (array->count == array->capacity) {
        return false;
    }

    u8* write_address = (u8*)array->memory + array->element_size * array->count;
    memcpy(write_address, element, array->element_size);

    array->count++;

    return true;
}

b8 d_array_pop(d_array* array, void* element) {
    if (array->count == 0) {
        return false;
    }

    const u8* read_address = (u8*)array->memory + array->element_size * array->count;
    memcpy(element, read_address, array->element_size);

    array->count--;

    return true;
}
