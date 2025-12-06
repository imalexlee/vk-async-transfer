#pragma once

#include "common.h"
#include "transfer_types.h"

b8 transfer_handle_pool_create(transfer_handle_pool* handle_pool);

void transfer_handle_pool_destroy(transfer_handle_pool* handle_pool);

b8 transfer_handle_pool_allocate_handle(transfer_handle_pool* handle_pool, transfer_handle* handle);

void transfer_handle_pool_reset_handle(transfer_handle_pool* handle_pool, transfer_handle handle);

void transfer_handle_pool_free_handle(transfer_handle_pool* handle_pool, transfer_handle handle);
