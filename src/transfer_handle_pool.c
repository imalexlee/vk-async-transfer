#include "transfer_handle_pool.h"

typedef enum handle_status {
    HANDLE_STATUS_ALLOCATED,
    HANDLE_STATUS_FREE,
} handle_status;

b8 transfer_handle_pool_create(transfer_handle_pool* handle_pool){
    assert(handle_pool);

    if(!d_array_create(&handle_pool->availability, sizeof(bool), 50)){
        return false;
    }
}