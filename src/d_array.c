#include "d_array.h"

static b8 d_array_realloc(d_array* array, u32 new_capacity) {
    void* temp = realloc(array->memory, new_capacity * array->element_size);
    if (!temp) {
        return false;
    }

    array->memory = temp;
    array->capacity = new_capacity;

    return true;
}

b8 d_array_resize(d_array* array, u32 new_count) {
    if (array->capacity < new_count) {
        if (!d_array_realloc(array, new_count)){
            return false;
        }
    }

    array->count = new_count;
    return true;
}

b8 d_array_create(d_array* array, u32 element_size, u32 initial_capacity){
    memset(array, 0, sizeof(d_array));
    array->element_size = element_size;

    if (!d_array_realloc(array, initial_capacity)){
        return false;
    }

    return true;
}

void d_array_destroy(d_array* array) {
    assert(array);

    free(array->memory);

    memset(array, 0, sizeof(d_array));
}

void* d_array_at(d_array* array, u32 index) {
    assert(array);
    assert(index < array->count);

    u32 memory_pos = array->element_size * index;
    return &array->memory[memory_pos];
}

b8 d_array_push_back(d_array* array, const void* value) {
    assert(array);
    assert(value);

    if (array->count == array->capacity) {
        if(!d_array_realloc(array, array->count * 2)) {
            return false;
        }
    }

    u32 memory_pos = array->element_size * array->count;
    memcpy(&array->memory[memory_pos], value, array->element_size);

    array->count++;

    return true;
}

b8 d_array_pop_back(d_array* array, void* value){
    if (array->count == 0) {
        return false;
    }

    array->count--;

    u32 memory_pos = array->element_size * array->count;
    memcpy(value, &array->memory[memory_pos], array->element_size);

    return true;
}
