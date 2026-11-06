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
#include <catch2/generators/catch_generators_range.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

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
    return to_bson(to_mongoac(bson.get()));
}

std::optional<bson_t> to_bson(mongoac_bson_t const& bson) {
    return to_bson(mongoac_bson_view_t{bson.ptr, bson.len});
}

std::optional<bson_t> to_bson(mongoac_bson_view_t bson) {
    if (bson_t ret = {}; bson.ptr != nullptr && bson_init_static(&ret, bson.ptr, bson.len)) {
        return ret;
    }

    return std::nullopt;
}

std::string as_json(unique_bson_ptr const& bson) {
    return as_json(to_mongoac(bson.get()));
}

std::string as_json(mongoac_bson_t const& bson) {
    return as_json(mongoac_bson_view_t{bson.ptr, bson.len});
}

std::string as_json(mongoac_bson_view_t bson) {
    if (auto const opt = to_bson(bson)) {
        bson_t const& doc = *opt;
        return as_json(doc);
    }

    return {};
}

std::string as_json(bson_t const& bson) {
    if (bson_get_data(&bson) == nullptr) {
        return {};
    }

    size_t len = {};
    auto const ptr = bson_as_canonical_extended_json(&bson, &len);
    auto ret = std::string(ptr, len);
    bson_free(ptr);
    return ret;
}

bson_t* operator""_bson(char const* ptr, std::size_t len) {
    return bson_new_from_json(reinterpret_cast<std::uint8_t const*>(ptr), static_cast<ssize_t>(len), nullptr);
}

TEST_CASE("as_json", "[mongoac][test]") {
    SECTION("null") {
        std::string_view const expected; // Empty.

        SECTION("unique_bson_ptr") {
            auto const doc = make_unique<bson_t>(nullptr, &bson_destroy);
            CHECK(as_json(doc) == expected);
        }

        SECTION("mongoac_bson_t") {
            mongoac_bson_t const bson = {};
            CHECK(as_json(bson) == expected);
        }

        SECTION("mongoac_bson_view_t") {
            mongoac_bson_view_t const bson = {};
            CHECK(as_json(bson) == expected);
        }

        SECTION("bson_t") {
            bson_t bson = {};
            CHECK(as_json(bson) == expected);
        }
    }

    SECTION("basic") {
        auto const entry = GENERATE(
            table<std::string_view, std::string_view>({
                {R"({})", R"({ })"},
                {R"({"x": "1"})", R"({ "x" : "1" })"},
            }));

        // Capturing structured bindings in `REQUIRE_MAKE_UNIQUE` requires C++20 or newer.
        auto const doc_str = std::get<0>(entry);
        auto const expected = std::get<1>(entry);
        auto const doc = REQUIRE_MAKE_UNIQUE(operator""_bson(doc_str.data(), doc_str.size()), &bson_destroy);

        SECTION("unique_bson_ptr") {
            CHECK(as_json(doc) == expected);
        }

        SECTION("mongoac_bson_t") {
            mongoac_bson_t const bson = {bson_get_data(doc.get()), doc->len};
            CHECK(as_json(bson) == expected);
        }

        SECTION("mongoac_bson_view_t") {
            mongoac_bson_view_t const bson = to_mongoac(doc.get());
            CHECK(as_json(bson) == expected);
        }

        SECTION("bson_t") {
            CHECK(as_json(*doc) == expected);
        }
    }
}

} // namespace mongoac::test
