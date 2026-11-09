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

namespace mongoac::test {

using unique_bson_ptr = std::unique_ptr<bson_t, void (*)(bson_t*)>;

mongoac_bson_view_t to_mongoac(bson_t const* bson);

unique_bson_ptr from_mongoac(mongoac_bson_t const& bson);
unique_bson_ptr from_mongoac(mongoac_bson_t&& bson);

std::optional<bson_t> to_bson(unique_bson_ptr const& bson);
std::optional<bson_t> to_bson(unique_bson_ptr&& bson) = delete;
std::optional<bson_t> to_bson(mongoac_bson_t const& bson);
std::optional<bson_t> to_bson(mongoac_bson_t&& bson) = delete;
std::optional<bson_t> to_bson(mongoac_bson_view_t bson);

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
