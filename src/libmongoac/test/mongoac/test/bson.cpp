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

#include <mongoac/test/bson.hh>

//

#include <mongoac/test/memory.hh>

#include <mongoac/bson.h>

#include <bson/bson.h>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

using mongoac::test::make_unique;

namespace mongoac::test {

mongoac_bson_view_t to_mongoac(bson_t const* bson) {
    if (bson == nullptr) {
        return {};
    }

    return {bson_get_data(bson), bson->len};
}

unique_bson_ptr from_mongoac(mongoac_bson_t const& bson) {
    if (!bson.ptr) {
        return {nullptr, &bson_destroy};
    }

    // Copy only.
    return make_unique(bson_new_from_data(bson.ptr, bson.len), &bson_destroy);
}

unique_bson_ptr from_mongoac(mongoac_bson_t&& bson) {
    if (!bson.ptr) {
        return {nullptr, &bson_destroy};
    }

    // Copy then destroy.
    auto ret = make_unique(bson_new_from_data(bson.ptr, bson.len), &bson_destroy);
    mongoac_bson_destroy(bson);
    return ret;
}

std::optional<bson_t> to_bson(unique_bson_ptr const& bson) {
    if (bson_t ret = {}; bson_init_static(&ret, bson_get_data(bson.get()), bson->len)) {
        return ret;
    }

    return std::nullopt;
}

std::optional<bson_t> to_bson(mongoac_bson_t const& bson) {
    if (bson_t ret = {}; bson_init_static(&ret, bson.ptr, bson.len)) {
        return ret;
    }

    return std::nullopt;
}

std::optional<bson_t> to_bson(mongoac_bson_view_t bson) {
    if (bson_t ret = {}; bson_init_static(&ret, bson.ptr, bson.len)) {
        return ret;
    }

    return std::nullopt;
}

bson_t* operator""_bson(char const* ptr, std::size_t len) {
    return bson_new_from_json(reinterpret_cast<std::uint8_t const*>(ptr), static_cast<ssize_t>(len), nullptr);
}

} // namespace mongoac::test
