# Copyright 2017 MongoDB Inc.
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

# Demonstrates how to use the CMake 'FindPkgConfig' mechanism to locate
# and build against libmongoc.

cmake_minimum_required(VERSION 2.8)

if (APPLE)
   cmake_policy(SET CMP0042 OLD)
endif()

project(hello_mongoc LANGUAGES C)

find_package(PkgConfig REQUIRED)

# NOTE: For this to work, the PKG_CONFIG_PATH variable must be set to point to
# the directory that was used as the argument to CMAKE_INSTALL_PREFIX when
# building libmongoc.
pkg_search_module(BSON REQUIRED libbson-1.0)
pkg_search_module(MONGOC REQUIRED libmongoc-1.0)

link_directories(${MONGOC_LIBRARY_DIRS} ${BSON_LIBRARY_DIRS})
add_executable(hello_mongoc hello_mongoc.c)
target_include_directories(hello_mongoc PRIVATE ${MONGOC_INCLUDE_DIRS} ${BSON_INCLUDE_DIRS})
target_link_libraries(hello_mongoc PRIVATE ${MONGOC_LIBRARIES} ${BSON_LIBRARIES})
