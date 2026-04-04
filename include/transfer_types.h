#pragma once

#include "common.h"
#include "d_array.h"
#include "d_queue.h"

/*
#define CMD_BUF_COUNT 5
#define QUEUE_ENTRIES_COUNT 100
#define TRANSFER_HANDLE_INVALID UINT32_MAX

typedef enum transfer_type {
    TRANSFER_TYPE_BUFFER_TO_BUFFER,
} transfer_type;

typedef union transfer_location {
    VkBuffer buffer;
    VkImage  image;
} transfer_location;

typedef enum transfer_internal_error {
    TRANSFER_INTERNAL_ERROR_NONE,
    TRANSFER_INTERNAL_ERROR_PTHREAD_CANNOT_CREATE,
    TRANSFER_INTERNAL_ERROR_CANT_POP_REQUEST,
} transfer_internal_error;

typedef enum transfer_error_type {
    TRANSFER_ERROR_TYPE_NONE,
    TRANSFER_ERROR_TYPE_INTERNAL,
    TRANSFER_ERROR_TYPE_VULKAN,
} transfer_error_type;

typedef struct transfer_error {
    transfer_error_type     type;
    transfer_internal_error internal_error;
    VkResult                vk_error;
} transfer_error;

typedef enum transfer_status {
    TRANSFER_STATUS_READY,
    TRANSFER_STATUS_PENDING,
    TRANSFER_STATUS_EXECUTING,
    TRANSFER_STATUS_COMPLETE,
    TRANSFER_STATUS_ERROR,
} transfer_status;

typedef u32 transfer_handle;

typedef struct buffer_to_buffer_request {
    VkBuffer src;
    VkBuffer dst;
    // Optional: Value of 0 indicates safest but possibly the slowest barriers
    VkAccessFlags        dst_access_mask;
    VkPipelineStageFlags dst_stage_mask;
    // Optional: pass handle if you care to check the status of this transfer.
    // Pass TRANSFER_HANDLE_INVALID to ignore
    transfer_handle handle;
} buffer_to_buffer_request;

typedef struct transfer_request {
    transfer_handle      handle;
    transfer_location    src;
    transfer_location    dst;
    transfer_type        type;
    VkAccessFlags        dst_access_mask;
    VkPipelineStageFlags dst_stage_mask;
} transfer_request;

typedef struct transfer_request_queue {
    d_queue         queue;
    pthread_cond_t  worker_notify_cond;
    pthread_mutex_t mutex;
} transfer_request_queue;

typedef struct transfer_commands {
    VkCommandPool        vk_cmd_pool;
    VkCommandBuffer      vk_cmd_bufs[CMD_BUF_COUNT];
    VkFence              vk_fences[CMD_BUF_COUNT];
    atomic_uint_fast64_t fence_generations[CMD_BUF_COUNT];
} transfer_command_pool;

typedef struct transfer_handle_fence_ref {
    VkFence vk_fence;
    u64     fence_generation;
    u32     fence_idx;
} transfer_handle_fence_ref;

typedef struct transfer_handle_pool {
    d_array available_indices;
    d_array handle_slots;
} transfer_handle_pool;

typedef struct transfer_engine {
    VkDevice               vk_device;
    VkQueue                vk_queue;
    transfer_command_pool  command_pool;
    transfer_handle_pool   handle_pool;
    transfer_request_queue request_queue;

    pthread_t worker_thread;

    atomic_bool should_close;
} transfer_engine;
*/

#define MAX_REQUESTS_PER_BATCH 64
#define MAX_BATCHES 16

// internal
typedef union transfer_location {
    VkBuffer vk_buffer;
    VkImage  vk_image;
} transfer_location;

// internal
typedef enum transfer_type {
    TRANSFER_TYPE_BUFFER_TO_BUFFER,
} transfer_type;

// internal
typedef struct transfer_request {
    transfer_location    src;
    transfer_location    dst;
    transfer_type        type;
    VkAccessFlags        dst_access_mask;
    VkPipelineStageFlags dst_stage_mask;
} transfer_request;

typedef void (*transfer_callback)(void*);

// allows user to set a callback to handle when these transfers finish
typedef struct transfer_batch {
    transfer_request  requests[MAX_REQUESTS_PER_BATCH];
    transfer_callback callback;
    void*             user_data;
} transfer_batch;

typedef enum vkt_internal_error {
    VKT_INTERNAL_ERROR_NONE,
    VKT_INTERNAL_ERROR_CANT_POP_REQUEST,
} vkt_internal_error;

typedef enum vkt_error_type {
    VKT_ERROR_TYPE_NONE,
    VKT_ERROR_TYPE_INTERNAL,
    VKT_ERROR_TYPE_VULKAN,
} vkt_error_type;

typedef struct vkt_error {
    vkt_error_type     type;
    vkt_internal_error internal_error;
    VkResult           vk_error;
} vkt_error;

// internal
typedef struct transfer_command {
    VkCommandPool   vk_cmd_pool;
    VkCommandBuffer vk_cmd_buf;
    VkFence         vk_fence;
} transfer_command;

typedef struct vkt {
    VkDevice         vk_device;
    VkQueue          vk_queue;
    transfer_command cmd;
    d_queue          transfer_batch_queue;
} vkt;
