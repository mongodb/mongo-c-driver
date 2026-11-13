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

#include <mongoac/bson.h>

#include <bson/bson_t.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

namespace mongoac::test {

// Suppress noisy `-Wignored-attributes` warnings due to using `bson_t` for template arguments.
#if defined(__GNUC__) && !defined(__clang__)
#define MONGOAC_TEST_DISABLE_IGNORED_ATTRIBUTES_GCC_PUSH() \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wignored-attributes\"") static_assert(true)
#define MONGOAC_TEST_DISABLE_IGNORED_ATTRIBUTES_GCC_POP() _Pragma("GCC diagnostic pop") static_assert(true)
#else
#define MONGOAC_TEST_DISABLE_IGNORED_ATTRIBUTES_GCC_PUSH() static_assert(true)
#define MONGOAC_TEST_DISABLE_IGNORED_ATTRIBUTES_GCC_POP() static_assert(true)
#endif

MONGOAC_TEST_DISABLE_IGNORED_ATTRIBUTES_GCC_PUSH();
using unique_bson_ptr = std::unique_ptr<bson_t, void (*)(bson_t*)>;

mongoac_bson_view_t to_mongoac(bson_t const* bson);

unique_bson_ptr from_mongoac(mongoac_bson_t const& bson);
unique_bson_ptr from_mongoac(mongoac_bson_t&& bson);

std::optional<bson_t> to_bson(unique_bson_ptr const& bson);
std::optional<bson_t> to_bson(unique_bson_ptr&& bson) = delete;
std::optional<bson_t> to_bson(mongoac_bson_t const& bson);
std::optional<bson_t> to_bson(mongoac_bson_t&& bson) = delete;
std::optional<bson_t> to_bson(mongoac_bson_view_t bson);
MONGOAC_TEST_DISABLE_IGNORED_ATTRIBUTES_GCC_POP();

std::string as_json(unique_bson_ptr const& bson);
std::string as_json(unique_bson_ptr&& bson) = delete;
std::string as_json(mongoac_bson_t const& bson);
std::string as_json(mongoac_bson_t&& bson) = delete;
std::string as_json(mongoac_bson_view_t bson);
std::string as_json(bson_t const& bson);

bson_t* operator""_bson(char const* ptr, std::size_t len);

// Return a `bson_t` (static-initialized, non-owning, read-only).
#define REQUIRE_BSON_STATIC(bson)       \
    [&]() -> bson_t {                   \
        auto const opt = to_bson(bson); \
        REQUIRE(opt.has_value());       \
        return *opt;                    \
    }()

// Return a `bson_iter_t` to the requested field.
#define REQUIRE_BSON_ITER_FIND(bson, key)                \
    [&]() -> bson_iter_t {                               \
        bson_iter_t iter = {};                           \
        REQUIRE(bson_iter_init_find(&iter, &bson, key)); \
        return iter;                                     \
    }()

} // namespace mongoac::test
