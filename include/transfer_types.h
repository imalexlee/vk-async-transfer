#pragma once

#include "common.h"
#include "d_queue.h"
#include "d_array.h"
#include "transfer_handle_pool.h"

#define CMD_BUF_COUNT 5
#define QUEUE_ENTRIES_COUNT 100

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

//typedef struct transfer_handle_t* transfer_handle;
typedef u32 transfer_handle;

typedef struct buffer_to_buffer_request {
    transfer_handle handle;
    VkBuffer        src;
    VkBuffer        dst;
    // Optional. Value of 0 indicates safest but possibly the slowest barriers
    VkAccessFlags        dst_access_mask;
    VkPipelineStageFlags dst_stage_mask;
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

typedef struct transfer_command_pool {
    VkCommandPool   pool;
    VkCommandBuffer buffers[CMD_BUF_COUNT];
    VkFence         fences[CMD_BUF_COUNT];
} transfer_command_pool;

typedef struct transfer_handle_pool {
    d_array available_indices;
    d_array handles;
} transfer_handle_pool;

typedef struct transfer_engine {
    VkDevice               vk_device;
    VkQueue                vk_queue;
    transfer_command_pool  command_pool;
    transfer_handle_pool handle_pool;
    transfer_request_queue request_queue;

    pthread_t worker_thread;

    atomic_bool should_close;
} transfer_engine;

