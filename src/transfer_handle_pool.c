#include "transfer_handle_pool.h"
#include "transfer_types.h"

typedef struct transfer_handle_t {
    _Atomic transfer_status status;
    transfer_error          error;
    VkFence                 vk_fence;
} transfer_handle_t;

typedef struct transfer_handle_slot_t {
    transfer_handle_t handle;
    bool              in_use;
} transfer_handle_slot_t;

static transfer_error default_error = {
    .type           = TRANSFER_ERROR_TYPE_NONE,
    .internal_error = TRANSFER_INTERNAL_ERROR_NONE,
    .vk_error       = VK_SUCCESS,
};

static transfer_handle_t default_handle = {
    .status   = TRANSFER_STATUS_READY,
    .error    = default_error,
    .vk_fence = VK_NULL_HANDLE,
};

static b8 allocate_more_handles(transfer_handle_pool* handle_pool, u32 new_count) {
    assert(handle_pool);

    u32 old_count = handle_pool->handle_slots.count;

    if (!d_array_resize(&handle_pool->handle_slots, new_count)) {
        return false;
    }

    if (!d_array_resize(&handle_pool->available_indices, new_count)) {
        return false;
    }

    transfer_handle_slot_t default_slot = {
        .handle = default_handle,
        .in_use = false,
    };

    for (i32 i = old_count; i < new_count; ++i) {
        transfer_handle_slot_t* slot = d_array_at(&handle_pool->handle_slots, i);
        *slot                        = default_slot;

        u32* free_index = d_array_at(&handle_pool->available_indices, i);
        *free_index     = i;
    }

    return true;
}

b8 transfer_handle_pool_create(transfer_handle_pool* handle_pool) {
    assert(handle_pool);

    const u32 init_capacity = 50;
    if (!d_array_create(&handle_pool->available_indices, sizeof(u32), init_capacity)) {
        return false;
    }

    if (!d_array_create(&handle_pool->handle_slots, sizeof(transfer_handle_slot_t), init_capacity)) {
        return false;
    }

    return allocate_more_handles(handle_pool, init_capacity);
}

void transfer_handle_pool_destroy(transfer_handle_pool* handle_pool) {
    assert(handle_pool);

    d_array_destroy(&handle_pool->handle_slots);
    d_array_destroy(&handle_pool->available_indices);
}

b8 transfer_handle_pool_allocate_handle(transfer_handle_pool* handle_pool, transfer_handle* handle) {
    assert(handle_pool);
    assert(handle);

    u32 free_index;
    if (!d_array_pop_back(&handle_pool->available_indices, &free_index)) {
        if (!allocate_more_handles(handle_pool, handle_pool->handle_slots.count * 2)) {
            return false;
        }
    }

    transfer_handle_slot_t* slot = d_array_at(&handle_pool->handle_slots, free_index);
    slot->in_use                 = true;

    *handle = free_index;

    return true;
}

void transfer_handle_pool_reset_handle(transfer_handle_pool* handle_pool, transfer_handle handle) {
    assert(handle_pool);
    assert(handle);
    assert(handle < handle_pool->handle_slots.count);

    transfer_handle_slot_t* slot = d_array_at(&handle_pool->handle_slots, handle);

    if (!slot->in_use) {
        return;
    }

    slot->handle = default_handle;
}

void transfer_handle_pool_free_handle(transfer_handle_pool* handle_pool, transfer_handle handle) {
    assert(handle_pool);
    assert(handle);
    assert(handle < handle_pool->handle_slots.count);

    transfer_handle_pool_reset_handle(handle_pool, handle);

    transfer_handle_slot_t* slot = d_array_at(&handle_pool->handle_slots, handle);
    slot->in_use                 = false;

    // should not fail considering that if we're here, then we've popped an available index
    // when we originally allocated a transfer handle.
    d_array_push_back(&handle_pool->available_indices, &handle);
}
