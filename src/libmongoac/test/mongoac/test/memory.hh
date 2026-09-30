// Copyright 2009-present MongoDB, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <memory>

namespace mongoac::test {

template <typename T, typename D = void (*)(T*)>
std::unique_ptr<T, D> make_unique(T* ptr, D deleter) {
    return std::unique_ptr<T, D>(ptr, deleter);
}

#define REQUIRE_MAKE_UNIQUE(ptr, deleter)                                          \
    [&] {                                                                          \
        auto _ptr = (ptr);                                                         \
        auto _deleter = (deleter);                                                 \
        REQUIRE(_ptr);                                                             \
        return ::mongoac::test::make_unique(std::move(_ptr), std::move(_deleter)); \
    }()

} // namespace mongoac::test
