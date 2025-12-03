#pragma once

#include "common.h"

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

typedef struct transfer_handle transfer_handle;

typedef struct buffer_to_buffer_request {
    transfer_handle* handle;
    VkBuffer         src;
    VkBuffer         dst;
} buffer_to_buffer_request;

typedef struct transfer_request {
    transfer_handle*  handle;
    transfer_location src;
    transfer_location dst;
    transfer_type     type;
} transfer_request;

typedef struct transfer_request_queue {
    i32              front;
    i32              back;
    transfer_request queue[QUEUE_ENTRIES_COUNT];

    pthread_cond_t  worker_notify_cond;
    pthread_mutex_t mutex;
} transfer_request_queue;

typedef struct transfer_command_pool {
    VkCommandPool   pool;
    VkCommandBuffer buffers[CMD_BUF_COUNT];
    VkFence         fences[CMD_BUF_COUNT];
} transfer_command_pool;

typedef struct transfer_engine {
    VkDevice               vk_device;
    VkQueue                vk_queue;
    transfer_command_pool  command_pool;
    transfer_request_queue request_queue;

    pthread_t worker_thread;

    atomic_bool should_close;
} transfer_engine;

b8 transfer_engine_init(transfer_engine* engine, VkDevice device, u32 transfer_queue_family, transfer_error* error);

void transfer_engine_copy_buffer_to_buffer(transfer_engine* engine, const buffer_to_buffer_request* buffer_transfer);

void transfer_engine_deinit(transfer_engine* engine);

transfer_handle* transfer_handle_create();

transfer_status transfer_handle_status(const transfer_engine* engine, transfer_handle* handle);

void transfer_handle_reset(transfer_handle* handle);

void transfer_handle_destroy(transfer_handle* handle);
