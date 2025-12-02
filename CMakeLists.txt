cmake_minimum_required(VERSION 3.31)
project(async_transfer_engine C)

set(CMAKE_C_STANDARD 17)

find_package(Vulkan REQUIRED)

add_subdirectory(src)
