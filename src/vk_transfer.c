
#include <vulkan/vulkan.h>
#include "vk_transfer.h"
#include <stdatomic.h>


static void enqueue_request(transfer_engine* engine, const transfer_request* request) {
    transfer_request_queue* queue = &engine->request_queue;
    pthread_mutex_lock(&queue->mutex);

    u32 idx           = queue->back;
    queue->back       = (queue->back + 1) % QUEUE_ENTRIES_COUNT;
    queue->queue[idx] = *request;

    pthread_cond_signal(&queue->worker_notify_cond);
    pthread_mutex_unlock(&queue->mutex);
}

static transfer_request dequeue_request(transfer_engine* engine) {
    transfer_request_queue* queue = &engine->request_queue;

    pthread_mutex_lock(&queue->mutex);

    while (queue->back == queue->front) {
        // queue is empty
        pthread_cond_wait(&queue->worker_notify_cond, &queue->mutex);
    }

    u32 idx      = queue->front;
    queue->front = (queue->front + 1) % QUEUE_ENTRIES_COUNT;

    transfer_request curr_request = queue->queue[idx];

    pthread_mutex_unlock(&queue->mutex);

    return curr_request;
}

static i32 get_available_command_buffer_idx(const transfer_engine* engine) {

    i32 i = 0;
    while (1) {
        VkResult vk_res = vkGetFenceStatus(engine->vk_device, engine->command_pool.fences[i]);
        if (vk_res == VK_SUCCESS) {
            // current fence is signaled and ready
            continue;
        }
        // TODO handle error codes
        i = (i + 1) % CMD_BUF_COUNT;
    }

    return i;
}

void* worker(void* arg) {
    transfer_engine* engine = arg;

    while (!atomic_load(&engine->should_close)) {
        transfer_request req = dequeue_request(engine);

        i32 cmd_idx = get_available_command_buffer_idx(engine);

        VkFence fence       = engine->command_pool.fences[cmd_idx];
        VkCommandBuffer cmd = engine->command_pool.buffers[cmd_idx];

        vkResetFences(engine->vk_device, 1, &fence);

        VkCommandBufferBeginInfo cmd_buf_bi;
        cmd_buf_bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &cmd_buf_bi);

        // TODO: perform transfer depending on request type

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit_info;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers    = &cmd;

        vkQueueSubmit(engine->vk_queue, 1, &submit_info, fence);
    }

    return NULL;
}


void transfer_engine_init(transfer_engine* engine, VkDevice device, u32 transfer_queue_family) {
    VkCommandPoolCreateInfo pool_ci;
    pool_ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_ci.queueFamilyIndex = transfer_queue_family;

    // TODO: handle vulkan errors
    vkCreateCommandPool(device, &pool_ci, NULL, &engine->command_pool.pool);

    VkCommandBufferAllocateInfo command_buffer_ai;
    command_buffer_ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_ai.commandBufferCount = CMD_BUF_COUNT;
    command_buffer_ai.commandPool        = engine->command_pool.pool;
    command_buffer_ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    vkAllocateCommandBuffers(device, &command_buffer_ai, engine->command_pool.buffers);

    vkGetDeviceQueue(device, transfer_queue_family, 0, &engine->vk_queue);

    VkFenceCreateInfo fence_ci;
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (i32 i = 0; i < CMD_BUF_COUNT; ++i) {
        vkCreateFence(device, &fence_ci, NULL, &engine->command_pool.fences[i]);
    }

    engine->request_queue.front = 0;
    engine->request_queue.back  = 0;

    engine->vk_device = device;

    pthread_create(&engine->worker_thread, NULL, worker, engine);
    pthread_cond_init(&engine->request_queue.worker_notify_cond, NULL);
    pthread_mutex_init(&engine->request_queue.mutex, NULL);

}
