#include "transfer_handle_pool.h"
#include "transfer_types.h"

typedef enum handle_availability {
    HANDLE_AVAILABILITY_ALLOCATED,
    HANDLE_AVAILABILITY_FREE,
} handle_availability;

typedef struct transfer_handle_t {
    _Atomic transfer_status status;
    transfer_error          error;
    VkFence                 vk_fence;
} transfer_handle_t;

b8 transfer_handle_pool_create(transfer_handle_pool* handle_pool){
    assert(handle_pool);

    const u32 init_capacity = 50;
    if(!d_array_create(&handle_pool->available_indices, sizeof(u32), init_capacity)) {
        return false;
    }

    if(!d_array_create(&handle_pool->handles, sizeof(transfer_handle_t), init_capacity)) {
        return false;
    }

    d_array_resize(&handle_pool->handles, init_capacity);

    for (i32 i = 0; i < init_capacity; ++i) {
        u32 free_index = init_capacity - i;
        d_array_push_back(&handle_pool->available_indices, &free_index);
    }

    return true;
}

b8 transfer_handle_pool_allocate_handle(transfer_handle_pool* handle_pool, transfer_handle* handle) {
    // if we're full
    // look for first free handle.

    u32 free_index;
    if (!d_array_pop_back(&handle_pool->available_indices, &free_index)) {
        // allocate more handles
        // if can't return false
    }
}