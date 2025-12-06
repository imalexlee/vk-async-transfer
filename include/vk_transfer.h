#pragma once

#include "common.h"
#include "transfer_types.h"

b8 transfer_engine_init(transfer_engine* engine, VkDevice device, u32 transfer_queue_family, transfer_error* error);

void transfer_engine_copy_buffer_to_buffer(transfer_engine* engine, const buffer_to_buffer_request* buffer_transfer);

void transfer_engine_deinit(transfer_engine* engine);

void transfer_handle_status(const transfer_engine* engine, transfer_handle handle, transfer_status* status);

void transfer_handle_reset(transfer_handle handle);
