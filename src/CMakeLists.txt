file(GLOB_RECURSE project_sources "*.c")

add_library(async_transfer_engine STATIC ${project_sources})

include_directories(../include)
target_include_directories(async_transfer_engine PUBLIC ../include)

target_link_libraries(async_transfer_engine Vulkan::Vulkan)