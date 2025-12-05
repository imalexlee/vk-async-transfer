#pragma once

#include "common.h"
#include "d_array.h"

typedef struct transfer_handle_pool {
    d_array availability;
    d_array handles;
} transfer_handle_pool;
