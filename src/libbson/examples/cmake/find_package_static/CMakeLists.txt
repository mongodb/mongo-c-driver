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

# Demonstrates how to use the CMake 'find_package' mechanism to locate
# and build against libbson.

cmake_minimum_required (VERSION 3.15...4.0)

project (hello_bson LANGUAGES C)

# NOTE: For this to work, the CMAKE_PREFIX_PATH variable must be set to point to
# the directory that was used as the argument to CMAKE_INSTALL_PREFIX when
# building libbson.
# -- sphinx-include-start --
# Specify the minimum version you require.
find_package (bson 2.0 REQUIRED)

# The "hello_bson.c" sample program is shared among four tests.
add_executable (hello_bson ../../hello_bson.c)
target_link_libraries (hello_bson PRIVATE bson::static)
