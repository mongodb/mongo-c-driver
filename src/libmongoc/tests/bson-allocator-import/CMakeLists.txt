cmake_minimum_required (VERSION 3.15...4.0)
project(BsonAllocatorImportTestProject C)

include(CTest)

# Inputs:
#   LINK_TARGET  The imported libbson target to link, e.g. "bson::static"
if(NOT LINK_TARGET)
    message(FATAL_ERROR "LINK_TARGET is required")
endif()

find_package(bson REQUIRED)
if(NOT TARGET ${LINK_TARGET})
    message(FATAL_ERROR "Installed package bson did not provide ${LINK_TARGET}")
endif()

add_executable(bson-allocator main.c)
target_link_libraries(bson-allocator PRIVATE ${LINK_TARGET})
add_test(bson-allocator bson-allocator)
