# Copyright 2009-present MongoDB, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Demonstrates how to use the CMake 'find_package' mechanism to locate and build against
# libmongoac, mirroring the pattern of the libmongoc and libbson examples.
#
# This project may be configured either:
#   * standalone, against an installed mongoac and bson, or
#   * in-tree, via add_subdirectory() from the mongo-c-driver build.
#
# NOTE: For a standalone build, the CMAKE_PREFIX_PATH variable must be set to point to the
# directory that was used as the argument to CMAKE_INSTALL_PREFIX when building libmongoac.
# See README.md.

set (mongoac_examples_standalone OFF)
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
   set (mongoac_examples_standalone ON)

   cmake_minimum_required (VERSION 3.23...4.0)

   project (mongoac-examples LANGUAGES C CXX)
endif ()

enable_language (CXX)

# Prefer the targets of the enclosing build when available; otherwise, find the installed
# packages.
if (TARGET mongoac_shared)
   set (mongoac_target mongoac_shared)
elseif (TARGET mongoac_static)
   set (mongoac_target mongoac_static)
else ()
   find_package (mongoac 0.1.0 CONFIG REQUIRED)
   set (mongoac_target mongoac::shared)
endif ()

if (NOT TARGET bson::shared AND NOT TARGET bson::static)
   find_package (bson 2.0 CONFIG REQUIRED)
endif ()

if (TARGET bson::shared)
   set (bson_target bson::shared)
else ()
   set (bson_target bson::static)
endif ()

find_package (Threads REQUIRED)

add_executable (queue-processing queue-processing.cpp)
target_compile_features (queue-processing PRIVATE cxx_std_17)
target_link_libraries (queue-processing PRIVATE ${mongoac_target} ${bson_target} Threads::Threads)

if (NOT mongoac_examples_standalone)
   set_property (TARGET queue-processing PROPERTY EXCLUDE_FROM_ALL TRUE)

   if (TARGET mongo_c_driver_examples)
      add_dependencies (mongo_c_driver_examples queue-processing)
   endif ()
endif ()

if (MSVC)
   target_compile_options (queue-processing PRIVATE /W4)
else ()
   target_compile_options (queue-processing PRIVATE -Wall -Wextra -Wpedantic)
endif ()
