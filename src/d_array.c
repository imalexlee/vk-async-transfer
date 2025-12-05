#include "d_array.h"

b8 d_array_reserve(d_array* array, u32 new_capacity) {
    void* temp = malloc(new_capacity * array->element_size);
    if (!temp) {
        return false;
    }

    if (array->memory){
        memcpy(temp, array->memory, array->count * array->element_size);
        free(array->memory);
    }

    array->memory = temp;
    array->capacity = new_capacity;
    return true;
}

b8 d_array_resize(d_array* array, u32 new_count) {
    if (array->capacity < new_count) {
        if (!d_array_reserve(array, new_count)){
            return false;
        }
    }

    array->count = new_count;
    return true;
}

b8 d_array_create(d_array* array, u32 element_size, u32 initial_capacity){
    array->element_size = element_size;

    if (!d_array_reserve(array, initial_capacity)){
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

void d_array_set_value(d_array* array, u32 index, const void* value) {
    assert(array);
    assert(value);
    assert(index < array->count);

    u32 memory_pos = array->element_size * index;
    memcpy(&array->memory[memory_pos], value, array->element_size);
}

void d_array_get_value(d_array* array, u32 index, void* value) {
    assert(array);
    assert(value);
    assert(index < array->count);

    u32 memory_pos = array->element_size * index;
    memcpy(value, &array->memory[memory_pos], array->element_size);
}
