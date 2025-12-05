#include "vk_transfer.h"

typedef struct transfer_handle_t {
    _Atomic transfer_status status;
    transfer_error          error;
    VkFence                 vk_fence;
} transfer_handle_t;

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

static void fill_handle_error_vulkan(transfer_handle handle, VkResult vk_error) {
    if (handle) {
        handle->error = fill_vulkan_err(vk_error);
        atomic_store(&handle->status, TRANSFER_STATUS_ERROR);
    }
}

static void fill_handle_error_internal(transfer_handle handle, transfer_internal_error internal_error) {
    if (handle) {
        handle->error = fill_internal_err(internal_error);
        atomic_store(&handle->status, TRANSFER_STATUS_ERROR);
    }
}

static b8 enqueue_request(transfer_engine* engine, const transfer_request* request) {
    if (!request) {
        return false;
    }

    transfer_request_queue* request_queue = &engine->request_queue;
    pthread_mutex_lock(&request_queue->mutex);

    bool push_successful = d_queue_push(&request_queue->queue, request);

    if (request->handle) {
        request->handle->vk_fence = VK_NULL_HANDLE;
        atomic_store(&request->handle->status, TRANSFER_STATUS_PENDING);
    }

    pthread_cond_signal(&request_queue->worker_notify_cond);
    pthread_mutex_unlock(&request_queue->mutex);

    return push_successful;
}

static b8 dequeue_request(transfer_engine* engine, transfer_request* request) {
    if (!request) {
        return false;
    }

    transfer_request_queue* request_queue = &engine->request_queue;
    pthread_mutex_lock(&request_queue->mutex);

    while (request_queue->queue.count == 0 && !atomic_load(&engine->should_close)) {
        pthread_cond_wait(&request_queue->worker_notify_cond, &request_queue->mutex);
    }

    bool pop_successful = d_queue_pop(&request_queue->queue, request);

    pthread_mutex_unlock(&request_queue->mutex);

    return pop_successful;
}

static VkResult get_available_command_buffer_idx(const transfer_engine* engine, i32* cmd_idx) {
    i32 i = 0;
    while (1) {
        VkResult vk_res = vkGetFenceStatus(engine->vk_device, engine->command_pool.fences[i]);
        if (vk_res == VK_SUCCESS) {
            *cmd_idx = i;
            return VK_SUCCESS;
        }
        if (vk_res == VK_NOT_READY) {
            i = (i + 1) % CMD_BUF_COUNT;
            continue;
        }

        return vk_res;
    }
}

static void transfer_buffer_to_buffer(VkCommandBuffer cmd, const transfer_request* transfer_request) {
    VkBufferCopy buffer_copy = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = VK_WHOLE_SIZE,
    };

    VkAccessFlags dst_access = transfer_request->dst_access_mask;
    if (dst_access == 0) {
        dst_access = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    }

    VkPipelineStageFlags dst_stage = transfer_request->dst_stage_mask;
    if (dst_stage == 0) {
        dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    VkBufferMemoryBarrier buffer_memory_barrier = {
        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext               = NULL,
        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask       = dst_access,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = transfer_request->dst.buffer,
        .offset              = 0,
        .size                = VK_WHOLE_SIZE,
    };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, dst_stage, 0, 0, NULL, 1, &buffer_memory_barrier, 0, NULL);

    vkCmdCopyBuffer(cmd, transfer_request->src.buffer, transfer_request->dst.buffer, 1, &buffer_copy);
}

static void* worker(void* arg) {
    transfer_engine* engine = arg;

    while (!atomic_load(&engine->should_close)) {
        transfer_request req;
        if (!dequeue_request(engine, &req)) {
            fill_handle_error_internal(req.handle, TRANSFER_INTERNAL_ERROR_CANT_POP_REQUEST);
            continue;
        }

        if (atomic_load(&engine->should_close)) {
            break;
        }

        i32      cmd_idx;
        VkResult vk_res = get_available_command_buffer_idx(engine, &cmd_idx);

        if (vk_res != VK_SUCCESS) {
            fill_handle_error_vulkan(req.handle, vk_res);
            continue;
        }

        VkFence         fence = engine->command_pool.fences[cmd_idx];
        VkCommandBuffer cmd   = engine->command_pool.buffers[cmd_idx];

        vk_res = vkResetFences(engine->vk_device, 1, &fence);

        if (vk_res != VK_SUCCESS) {
            fill_handle_error_vulkan(req.handle, vk_res);
            continue;
        }

        VkCommandBufferBeginInfo cmd_buf_bi = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = NULL,
            .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = NULL,
        };

        vk_res = vkBeginCommandBuffer(cmd, &cmd_buf_bi);
        if (vk_res != VK_SUCCESS) {
            fill_handle_error_vulkan(req.handle, vk_res);
            continue;
        }

        switch (req.type) {
        case TRANSFER_TYPE_BUFFER_TO_BUFFER:
            transfer_buffer_to_buffer(cmd, &req);
            continue;
        default:
            assert(0 && "unhandled transfer type");
        }

        vk_res = vkEndCommandBuffer(cmd);

        if (vk_res != VK_SUCCESS) {
            fill_handle_error_vulkan(req.handle, vk_res);
            continue;
        }

        VkSubmitInfo submit_info = {
            .sType                = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext                = NULL,
            .waitSemaphoreCount   = 0,
            .pWaitSemaphores      = NULL,
            .pWaitDstStageMask    = NULL,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &cmd,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores    = NULL,
        };

        vk_res = vkQueueSubmit(engine->vk_queue, 1, &submit_info, fence);

        if (vk_res != VK_SUCCESS) {
            fill_handle_error_vulkan(req.handle, vk_res);
            continue;
        }

        if (req.handle) {
            req.handle->vk_fence = fence;
            atomic_store(&req.handle->status, TRANSFER_STATUS_EXECUTING);
        }
    }

    return NULL;
}

b8 transfer_engine_init(transfer_engine* engine, VkDevice device, u32 transfer_queue_family, transfer_error* error) {
    assert(engine);
    assert(device != VK_NULL_HANDLE);

    atomic_store(&engine->should_close, false);

    VkCommandPoolCreateInfo pool_ci;
    pool_ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_ci.queueFamilyIndex = transfer_queue_family;

    VkResult vk_res;
    vk_res = vkCreateCommandPool(device, &pool_ci, NULL, &engine->command_pool.pool);

    if (vk_res != VK_SUCCESS) {
        if (error) {
            *error = fill_vulkan_err(vk_res);
        }
        transfer_engine_deinit(engine);
        return false;
    }

    VkCommandBufferAllocateInfo command_buffer_ai = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = NULL,
        .commandPool        = engine->command_pool.pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = CMD_BUF_COUNT,
    };

    vk_res = vkAllocateCommandBuffers(device, &command_buffer_ai, engine->command_pool.buffers);

    if (vk_res != VK_SUCCESS) {
        if (error) {
            *error = fill_vulkan_err(vk_res);
        }
        transfer_engine_deinit(engine);
        return false;
    }

    vkGetDeviceQueue(device, transfer_queue_family, 0, &engine->vk_queue);

    VkFenceCreateInfo fence_ci = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (i32 i = 0; i < CMD_BUF_COUNT; ++i) {
        vk_res = vkCreateFence(device, &fence_ci, NULL, &engine->command_pool.fences[i]);

        if (vk_res != VK_SUCCESS) {
            if (error) {
                *error = fill_vulkan_err(vk_res);
            }
            transfer_engine_deinit(engine);
            return false;
        }
    }

    d_queue_create(&engine->request_queue.queue, sizeof(transfer_request), 50);

    engine->vk_device = device;

    i32 pthread_res;
    pthread_res = pthread_create(&engine->worker_thread, NULL, worker, engine);
    pthread_res = pthread_cond_init(&engine->request_queue.worker_notify_cond, NULL);
    pthread_res = pthread_mutex_init(&engine->request_queue.mutex, NULL);

    if (vk_res != VK_SUCCESS) {
        if (error) {
            *error = fill_vulkan_err(vk_res);
        }
        transfer_engine_deinit(engine);
        return false;
    }
    if (pthread_res != 0) {
        if (error) {
            *error = fill_internal_err(TRANSFER_INTERNAL_ERROR_PTHREAD_CANNOT_CREATE);
        }
        transfer_engine_deinit(engine);
        return false;
    }

    return true;
}

void transfer_engine_deinit(transfer_engine* engine) {
    atomic_store(&engine->should_close, true);

    pthread_mutex_lock(&engine->request_queue.mutex);
    pthread_cond_broadcast(&engine->request_queue.worker_notify_cond);
    pthread_mutex_unlock(&engine->request_queue.mutex);

    pthread_mutex_destroy(&engine->request_queue.mutex);
    pthread_cond_destroy(&engine->request_queue.worker_notify_cond);

    pthread_join(engine->worker_thread, NULL);

    d_queue_destroy(&engine->request_queue.queue);

    for (i32 i = 0; i < CMD_BUF_COUNT; ++i) {
        vkDestroyFence(engine->vk_device, engine->command_pool.fences[i], NULL);
    }

    vkDestroyCommandPool(engine->vk_device, engine->command_pool.pool, NULL);
}

void transfer_engine_copy_buffer_to_buffer(transfer_engine* engine, const buffer_to_buffer_request* buffer_transfer) {
    transfer_handle_reset(buffer_transfer->handle);

    transfer_request transfer_request = {
        .handle          = buffer_transfer->handle,
        .src             = buffer_transfer->src,
        .dst             = buffer_transfer->dst,
        .type            = TRANSFER_TYPE_BUFFER_TO_BUFFER,
        .dst_access_mask = buffer_transfer->dst_access_mask,
        .dst_stage_mask  = buffer_transfer->dst_stage_mask,
    };

    enqueue_request(engine, &transfer_request);
}

void transfer_handle_status(const transfer_engine* engine, transfer_handle handle, transfer_status* status) {
    if (!engine || !handle || !status) {
        return;
    }

    transfer_status handle_status = atomic_load(&handle->status);

    if (handle_status != TRANSFER_STATUS_EXECUTING) {
        *status = handle_status;
        return;
    }

    VkResult vk_res = vkGetFenceStatus(engine->vk_device, handle->vk_fence);
    switch (vk_res) {
    case VK_SUCCESS: {
        atomic_store(&handle->status, TRANSFER_STATUS_COMPLETE);
        *status = TRANSFER_STATUS_COMPLETE;
        return;
    }
    case VK_NOT_READY: {
        *status = TRANSFER_STATUS_EXECUTING;
        return;
    }
    default:
        fill_handle_error_vulkan(handle, vk_res);
        *status = TRANSFER_STATUS_ERROR;
    }
}

void transfer_handle_reset(transfer_handle handle) {
    if (!handle) {
        return;
    }

    transfer_error default_err = {
        .type           = TRANSFER_ERROR_TYPE_NONE,
        .internal_error = TRANSFER_INTERNAL_ERROR_NONE,
        .vk_error       = VK_SUCCESS,
    };

    handle->error    = default_err;
    handle->vk_fence = VK_NULL_HANDLE;
    atomic_store(&handle->status, TRANSFER_STATUS_READY);
}

void transfer_handle_create( transfer_handle* handle) {
    // TODO: add pooling strategy to avoid calloc
    if (!handle) {
        return;
    }

    *handle = calloc(1, sizeof(transfer_handle));

    if (!*handle) {
        return;
    }

    transfer_handle_reset(*handle);
}

void transfer_handle_destroy(transfer_handle handle) {
    // TODO: add pooling strategy to avoid free
    if (!handle) {
        return;
    };
    free(handle);
}