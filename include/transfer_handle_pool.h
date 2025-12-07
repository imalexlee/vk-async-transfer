#pragma once

#include "common.h"
#include "transfer_types.h"

b8 transfer_handle_pool_create(transfer_handle_pool* handle_pool);

void transfer_handle_pool_destroy(transfer_handle_pool* handle_pool);

b8 transfer_handle_pool_allocate_handle(transfer_handle_pool* handle_pool, transfer_handle* handle);

void transfer_handle_pool_reset_handle(transfer_handle_pool* handle_pool, transfer_handle handle);

void transfer_handle_pool_free_handle(transfer_handle_pool* handle_pool, transfer_handle handle);

void transfer_handle_pool_set_handle_error_vulkan(transfer_handle_pool* handle_pool, transfer_handle handle, VkResult vk_error);

void transfer_handle_pool_set_handle_error_internal(transfer_handle_pool* handle_pool, transfer_handle handle,
                                                    transfer_internal_error internal_error);

void transfer_handle_pool_set_handle_fence(transfer_handle_pool* handle_pool, transfer_handle handle, VkFence fence, u64 fence_generation,
                                           u32 fence_idx);

void transfer_handle_pool_insert_status_barrier(transfer_handle_pool* handle_pool, transfer_handle handle, transfer_status status);

b8 transfer_handle_pool_get_handle_status(transfer_handle_pool* handle_pool, transfer_handle handle, transfer_status* status);

transfer_handle_fence_ref* transfer_handle_pool_get_handle_fence_ref(transfer_handle_pool* handle_pool, transfer_handle handle);
