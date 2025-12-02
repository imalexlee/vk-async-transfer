
#include "vk_transfer.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <vulkan/vulkan.h>

static void trigger_err_callback_vulkan(transfer_engine* engine, VkResult vk_error) {
    transfer_engine_deinit(engine);
    if (engine->error_callback != NULL) {
        transfer_error error;
        error.type           = TRANSFER_ERROR_TYPE_VULKAN;
        error.internal_error = TRANSFER_INTERNAL_ERROR_NONE;
        error.vk_error       = vk_error;
        engine->error_callback(error);
    }
}

static void trigger_err_callback_internal(transfer_engine* engine, transfer_internal_error internal_error) {
    transfer_engine_deinit(engine);
    if (engine->error_callback != NULL) {
        transfer_error error;
        error.type           = TRANSFER_ERROR_TYPE_INTERNAL;
        error.internal_error = internal_error;
        error.vk_error       = VK_SUCCESS;
        engine->error_callback(error);
    }
}

static void enqueue_request(transfer_engine* engine, const transfer_request* request) {
    transfer_request_queue* queue = &engine->request_queue;
    pthread_mutex_lock(&queue->mutex);

    queue->queue[queue->back] = *request;
    queue->back               = (queue->back + 1) % QUEUE_ENTRIES_COUNT;

    pthread_cond_signal(&queue->worker_notify_cond);
    pthread_mutex_unlock(&queue->mutex);
}

static transfer_request dequeue_request(transfer_engine* engine) {
    transfer_request_queue* queue = &engine->request_queue;

    pthread_mutex_lock(&queue->mutex);

    while (queue->back == queue->front && !atomic_load(&engine->should_close)) {
        // queue is empty
        pthread_cond_wait(&queue->worker_notify_cond, &queue->mutex);
    }

    transfer_request curr_request = queue->queue[queue->front];
    queue->front                  = (queue->front + 1) % QUEUE_ENTRIES_COUNT;

    pthread_mutex_unlock(&queue->mutex);

    return curr_request;
}

static i32 get_available_command_buffer_idx(transfer_engine* engine) {
    i32 i = 0;
    while (1) {
        VkResult vk_res = vkGetFenceStatus(engine->vk_device, engine->command_pool.fences[i]);
        if (vk_res == VK_SUCCESS) {
            // current fence is signaled and ready
            break;
        }
        if (vk_res == VK_NOT_READY) {
            i = (i + 1) % CMD_BUF_COUNT;
            continue;
        }

        trigger_err_callback_vulkan(engine, vk_res);
    }

    return i;
}

static void transfer_buffer_to_buffer(VkCommandBuffer cmd, const transfer_request* transfer_request) {
    VkBufferCopy buffer_copy;
    buffer_copy.srcOffset = 0;
    buffer_copy.dstOffset = 0;
    buffer_copy.size      = VK_WHOLE_SIZE;

    vkCmdCopyBuffer(cmd, transfer_request->src.buffer, transfer_request->dst.buffer, 1, &buffer_copy);
}

void* worker(void* arg) {
    transfer_engine* engine = arg;

    while (!atomic_load(&engine->should_close)) {
        transfer_request req = dequeue_request(engine);

        if (atomic_load(&engine->should_close)) {
            break;
        }

        i32 cmd_idx = get_available_command_buffer_idx(engine);

        VkFence         fence = engine->command_pool.fences[cmd_idx];
        VkCommandBuffer cmd   = engine->command_pool.buffers[cmd_idx];

        vkResetFences(engine->vk_device, 1, &fence);

        VkCommandBufferBeginInfo cmd_buf_bi;
        cmd_buf_bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &cmd_buf_bi);

        switch (req.type) {
        case TRANSFER_TYPE_BUFFER_TO_BUFFER:
            transfer_buffer_to_buffer(cmd, &req);
            break;

        default:
            assert(0 && "unhandled transfer type");
        }

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit_info;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers    = &cmd;

        vkQueueSubmit(engine->vk_queue, 1, &submit_info, fence);
    }

    return NULL;
}

void transfer_engine_init(transfer_engine* engine, VkDevice device, u32 transfer_queue_family, const transfer_error_callback* error_callback) {

    engine->error_callback = *error_callback;
    atomic_store(&engine->should_close, false);

    VkCommandPoolCreateInfo pool_ci;
    pool_ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_ci.queueFamilyIndex = transfer_queue_family;

    // TODO: handle vulkan errors
    VkResult vk_res;
    vk_res = vkCreateCommandPool(device, &pool_ci, NULL, &engine->command_pool.pool);

    if (vk_res != VK_SUCCESS) {
        trigger_err_callback_vulkan(engine, vk_res);
    }

    VkCommandBufferAllocateInfo command_buffer_ai;
    command_buffer_ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_ai.commandBufferCount = CMD_BUF_COUNT;
    command_buffer_ai.commandPool        = engine->command_pool.pool;
    command_buffer_ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    vk_res = vkAllocateCommandBuffers(device, &command_buffer_ai, engine->command_pool.buffers);

    if (vk_res != VK_SUCCESS) {
        trigger_err_callback_vulkan(engine, vk_res);
    }

    vkGetDeviceQueue(device, transfer_queue_family, 0, &engine->vk_queue);

    VkFenceCreateInfo fence_ci;
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (i32 i = 0; i < CMD_BUF_COUNT; ++i) {
        vk_res = vkCreateFence(device, &fence_ci, NULL, &engine->command_pool.fences[i]);

        if (vk_res != VK_SUCCESS) {
            trigger_err_callback_vulkan(engine, vk_res);
        }
    }

    engine->request_queue.front = 0;
    engine->request_queue.back  = 0;

    engine->vk_device = device;

    i32 pthread_res;
    pthread_res = pthread_create(&engine->worker_thread, NULL, worker, engine);
    pthread_res = pthread_cond_init(&engine->request_queue.worker_notify_cond, NULL);
    pthread_res = pthread_mutex_init(&engine->request_queue.mutex, NULL);

    if (pthread_res != 0) {
        trigger_err_callback_internal(engine, TRANSFER_INTERNAL_ERROR_PTHREAD_CANNOT_CREATE);
    }
}

void transfer_engine_deinit(transfer_engine* engine) {
    atomic_store(&engine->should_close, true);

    pthread_mutex_lock(&engine->request_queue.mutex);
    pthread_cond_broadcast(&engine->request_queue.worker_notify_cond);
    pthread_mutex_unlock(&engine->request_queue.mutex);

    pthread_mutex_destroy(&engine->request_queue.mutex);
    pthread_cond_destroy(&engine->request_queue.worker_notify_cond);

    pthread_join(engine->worker_thread, NULL);

    for (i32 i = 0; i < CMD_BUF_COUNT; ++i) {
        vkDestroyFence(engine->vk_device, engine->command_pool.fences[i], NULL);
    }

    vkDestroyCommandPool(engine->vk_device, engine->command_pool.pool, NULL);
}

void transfer_engine_copy_buffer_to_buffer(transfer_engine* engine, const buffer_to_buffer_request* buffer_transfer) {
    transfer_request transfer_request;
    transfer_request.type       = TRANSFER_TYPE_BUFFER_TO_BUFFER;
    transfer_request.src.buffer = buffer_transfer->src;
    transfer_request.dst.buffer = buffer_transfer->dst;

    enqueue_request(engine, &transfer_request);
}
