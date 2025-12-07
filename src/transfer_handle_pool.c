#include "transfer_handle_pool.h"
#include "transfer_types.h"

typedef struct transfer_handle_t {
    _Atomic transfer_status   status;
    transfer_error            error;
    transfer_handle_fence_ref fence_ref;
} transfer_handle_t;

typedef struct transfer_handle_slot_t {
    transfer_handle_t handle;
    bool              valid;
} transfer_handle_slot_t;

const transfer_error default_error = {
    .type           = TRANSFER_ERROR_TYPE_NONE,
    .internal_error = TRANSFER_INTERNAL_ERROR_NONE,
    .vk_error       = VK_SUCCESS,
};

const transfer_handle_t default_handle = {
    .status    = TRANSFER_STATUS_READY,
    .error     = default_error,
    .fence_ref = {.vk_fence = VK_NULL_HANDLE, .fence_generation = 0, .fence_idx = 0},
};

static transfer_handle_slot_t* transfer_handle_pool_get_handle_slot(transfer_handle_pool* handle_pool, transfer_handle handle) {
    assert(handle_pool);

    if (handle == TRANSFER_HANDLE_INVALID) {
        return NULL;
    }

    assert(handle < handle_pool->handle_slots.count);

    transfer_handle_slot_t* slot = d_array_at(&handle_pool->handle_slots, handle);

    if (!slot->valid) {
        return NULL;
    }

    return slot;
}

transfer_handle_fence_ref* transfer_handle_pool_get_handle_fence_ref(transfer_handle_pool* handle_pool, transfer_handle handle) {
    assert(handle_pool);
    assert(handle);

    transfer_handle_slot_t* handle_slot = transfer_handle_pool_get_handle_slot(handle_pool, handle);

    if (!handle_slot) {
        return NULL;
    }

    return &handle_slot->handle.fence_ref;
}

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
        .valid  = false,
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
    slot->valid                  = true;

    *handle = free_index;

    return true;
}

void transfer_handle_pool_reset_handle(transfer_handle_pool* handle_pool, transfer_handle handle) {
    assert(handle_pool);
    assert(handle);

    transfer_handle_slot_t* handle_slot = transfer_handle_pool_get_handle_slot(handle_pool, handle);

    if (!handle_slot) {
        return;
    }

    handle_slot->handle = default_handle;
}

void transfer_handle_pool_free_handle(transfer_handle_pool* handle_pool, transfer_handle handle) {
    assert(handle_pool);
    assert(handle);
    assert(handle < handle_pool->handle_slots.count);

    transfer_handle_pool_reset_handle(handle_pool, handle);

    transfer_handle_slot_t* slot = d_array_at(&handle_pool->handle_slots, handle);
    slot->valid                  = false;

    // should not fail considering that if we're here, then we've popped an available index
    // when we originally allocated a transfer handle.
    d_array_push_back(&handle_pool->available_indices, &handle);
}

static transfer_error fill_vulkan_err(VkResult vk_error) {
    transfer_error err;
    err.type           = TRANSFER_ERROR_TYPE_VULKAN;
    err.vk_error       = vk_error;
    err.internal_error = TRANSFER_INTERNAL_ERROR_NONE;
    return err;
}

static transfer_error fill_internal_err(transfer_internal_error internal_error) {
    transfer_error err;
    err.type           = TRANSFER_ERROR_TYPE_INTERNAL;
    err.vk_error       = VK_SUCCESS;
    err.internal_error = internal_error;
    return err;
}

void transfer_handle_pool_set_handle_error_vulkan(transfer_handle_pool* handle_pool, transfer_handle handle, VkResult vk_error) {
    transfer_handle_slot_t* handle_slot = transfer_handle_pool_get_handle_slot(handle_pool, handle);

    if (!handle_slot) {
        return;
    }

    handle_slot->handle.error = fill_vulkan_err(vk_error);
    transfer_handle_pool_insert_status_barrier(handle_pool, handle, TRANSFER_STATUS_ERROR);
}

void transfer_handle_pool_set_handle_error_internal(transfer_handle_pool* handle_pool, transfer_handle handle,
                                                    transfer_internal_error internal_error) {
    transfer_handle_slot_t* handle_slot = transfer_handle_pool_get_handle_slot(handle_pool, handle);

    if (!handle_slot) {
        return;
    }

    handle_slot->handle.error = fill_internal_err(internal_error);
    transfer_handle_pool_insert_status_barrier(handle_pool, handle, TRANSFER_STATUS_ERROR);
}

void transfer_handle_pool_set_handle_fence(transfer_handle_pool* handle_pool, transfer_handle handle, VkFence fence, u64 fence_generation,
                                           u32 fence_idx) {
    transfer_handle_slot_t* handle_slot = transfer_handle_pool_get_handle_slot(handle_pool, handle);

    if (!handle_slot) {
        return;
    }

    transfer_handle_fence_ref fence_ref = {
        .vk_fence         = fence,
        .fence_generation = fence_generation,
        .fence_idx        = fence_idx,
    };

    handle_slot->handle.fence_ref = fence_ref;
}

void transfer_handle_pool_insert_status_barrier(transfer_handle_pool* handle_pool, transfer_handle handle, transfer_status status) {
    transfer_handle_slot_t* handle_slot = transfer_handle_pool_get_handle_slot(handle_pool, handle);

    if (!handle_slot) {
        return;
    }

    atomic_store(&handle_slot->handle.status, status);
}

b8 transfer_handle_pool_get_handle_status(transfer_handle_pool* handle_pool, transfer_handle handle, transfer_status* status) {
    transfer_handle_slot_t* handle_slot = transfer_handle_pool_get_handle_slot(handle_pool, handle);

    if (!handle_slot) {
        return false;
    }

    *status = atomic_load(&handle_slot->handle.status);
    return true;
}
