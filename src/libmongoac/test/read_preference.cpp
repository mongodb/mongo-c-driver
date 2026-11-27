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

#include <mongoac/read_preference.h>

//

#include <mongoac/test/array.hh>
#include <mongoac/test/error.hh>
#include <mongoac/test/memory.hh>
#include <mongoac/test/string.hh>

#include <mongoac/error.h>
#include <mongoac/string.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <set>
#include <string>
#include <string_view>

using mongoac::test::from_mongoac;
using mongoac::test::make_unique;
using mongoac::test::operator""_sv;
using mongoac::test::to_array;
using mongoac::test::to_mongoac;

namespace {

constexpr auto mode_cases = to_array<mongoac_read_preference_mode_t>({
    MONGOAC_READ_PREFERENCE_MODE_PRIMARY,
    MONGOAC_READ_PREFERENCE_MODE_SECONDARY,
    MONGOAC_READ_PREFERENCE_MODE_PRIMARY_PREFERRED,
    MONGOAC_READ_PREFERENCE_MODE_SECONDARY_PREFERRED,
    MONGOAC_READ_PREFERENCE_MODE_NEAREST,
});

} // namespace

TEST_CASE("destroy", "[mongoac][read_preference]") {
    SECTION("null") {
        mongoac_read_preference_destroy(nullptr);
        SUCCEED();
    }
}

TEST_CASE("clone", "[mongoac][read_preference]") {
    SECTION("null") {
        auto const read_preference =
            make_unique(mongoac_read_preference_clone(nullptr), &mongoac_read_preference_destroy);

        CHECK(read_preference == nullptr);
    }

    SECTION("basic") {
        auto const mode = GENERATE(from_range(mode_cases));

        CAPTURE(mode);

        auto const source_owner = REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
        auto const source = source_owner.get();

        mongoac_read_preference_set_mode(source, mode);

        auto const target_owner = make_unique(mongoac_read_preference_clone(source), &mongoac_read_preference_destroy);
        auto const target = target_owner.get();

        CHECK(target != nullptr);
        CHECK(target != source);
        CHECK(mongoac_read_preference_get_mode(target) == mode);
    }
}

TEST_CASE("new", "[mongoac][read_preference]") {
    SECTION("default") {
        auto const read_preference_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
        auto const read_preference = read_preference_owner.get();

        CHECK(mongoac_read_preference_get_mode(read_preference) == MONGOAC_READ_PREFERENCE_MODE_PRIMARY);
        CHECK(mongoac_read_preference_get_max_staleness_is_none(read_preference));
    }
}

TEST_CASE("get_max_staleness_as_secs", "[mongoac][read_preference]") {
    SECTION("null") {
        CHECK(mongoac_read_preference_get_max_staleness_as_secs(nullptr) == std::uint64_t{}); // Default::default()
    }

    SECTION("default") {
        auto const v = GENERATE(from_range(mode_cases));

        CAPTURE(v);

        auto const read_preference_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
        auto const read_preference = read_preference_owner.get();

        mongoac_read_preference_set_mode(read_preference, v);

        CHECK(
            mongoac_read_preference_get_max_staleness_as_secs(read_preference) ==
            std::uint64_t{}); // Default::default()
    }
}

TEST_CASE("get_max_staleness_is_none", "[mongoac][read_preference]") {
    SECTION("null") {
        CHECK(mongoac_read_preference_get_max_staleness_is_none(nullptr));
    }

    SECTION("default") {
        auto const v = GENERATE(from_range(mode_cases));

        CAPTURE(v);

        auto const read_preference_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
        auto const read_preference = read_preference_owner.get();

        mongoac_read_preference_set_mode(read_preference, v);

        CHECK(mongoac_read_preference_get_max_staleness_is_none(read_preference));
    }
}

TEST_CASE("get_mode", "[mongoac][read_preference]") {
    SECTION("null") {
        CHECK(mongoac_read_preference_get_mode(nullptr) == MONGOAC_READ_PREFERENCE_MODE_UNKNOWN);
    }

    SECTION("basic") {
        auto const v = GENERATE(from_range(mode_cases));

        CAPTURE(v);

        auto const read_preference_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
        auto const read_preference = read_preference_owner.get();

        mongoac_read_preference_set_mode(read_preference, v);

        CHECK(mongoac_read_preference_get_mode(read_preference) == v);
    }
}

TEST_CASE("get_tag_sets_get_get", "[mongoac][read_preference]") {
    auto const read_preference_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
    auto const read_preference = read_preference_owner.get();

    auto const index = GENERATE(values<std::uintptr_t>({0u, 1u, UINTPTR_MAX - 1u, UINTPTR_MAX}));
    CAPTURE(index);

    SECTION("null") {
        CHECK(mongoac_read_preference_get_tag_sets_get_get(nullptr, index, {}).ptr == nullptr);
        CHECK(mongoac_read_preference_get_tag_sets_get_get(nullptr, index, "key"_sv).ptr == nullptr);
    }

    SECTION("default") {
        CHECK(mongoac_read_preference_get_tag_sets_get_get(read_preference, index, {}).ptr == nullptr);
        CHECK(mongoac_read_preference_get_tag_sets_get_get(read_preference, index, "key"_sv).ptr == nullptr);
    }
}

TEST_CASE("get_tag_sets_get_keys", "[mongoac][read_preference]") {
    auto const read_preference_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
    auto const read_preference = read_preference_owner.get();

    auto const index = GENERATE(values<std::uintptr_t>({0u, 1u, UINTPTR_MAX - 1u, UINTPTR_MAX}));
    CAPTURE(index);

    SECTION("null") {
        auto const result_owner = make_unique(mongoac_read_preference_get_tag_sets_get_keys(nullptr, index));
        auto const result = result_owner.value();

        CHECK(result.ptr == nullptr);
        CHECK(result.len == std::uintptr_t{}); // Default::default()
    }

    SECTION("default") {
        auto const keys_owner = make_unique(mongoac_read_preference_get_tag_sets_get_keys(read_preference, index));
        auto const keys = keys_owner.value();

        CHECK(keys.ptr == nullptr);
        CHECK(keys.len == 0u); // Default::default()
    }

    SECTION("basic") {
        auto const mode = GENERATE(from_range(mode_cases));
        CAPTURE(mode);

        mongoac_read_preference_set_mode(read_preference, mode);

        auto const keys_owner = make_unique(mongoac_read_preference_get_tag_sets_get_keys(read_preference, index));
        auto const keys = keys_owner.value();

        CHECK(keys.ptr == nullptr);
        CHECK(keys.len == 0u);
    }
}

TEST_CASE("get_tag_sets_get_len", "[mongoac][read_preference]") {
    auto const read_preference_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
    auto const read_preference = read_preference_owner.get();

    auto const index = GENERATE(values<std::uintptr_t>({0u, 1u, UINTPTR_MAX - 1u, UINTPTR_MAX}));
    CAPTURE(index);

    SECTION("null") {
        CHECK(mongoac_read_preference_get_tag_sets_get_len(nullptr, index) == std::uintptr_t{}); // Default::default()
    }

    SECTION("default") {
        CHECK(mongoac_read_preference_get_tag_sets_get_len(read_preference, index) == 0u);
    }

    SECTION("basic") {
        auto const mode = GENERATE(from_range(mode_cases));
        CAPTURE(mode);

        mongoac_read_preference_set_mode(read_preference, mode);

        CHECK(mongoac_read_preference_get_tag_sets_get_len(read_preference, index) == 0u);
    }
}

TEST_CASE("get_tag_sets_len", "[mongoac][read_preference]") {
    auto const read_preference_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
    auto const read_preference = read_preference_owner.get();

    SECTION("null") {
        CHECK(mongoac_read_preference_get_tag_sets_len(nullptr) == std::uintptr_t{}); // Default::default()
    }

    SECTION("default") {
        CHECK(mongoac_read_preference_get_tag_sets_len(read_preference) == 0u);
    }

    SECTION("basic") {
        auto const mode = GENERATE(from_range(mode_cases));
        CAPTURE(mode);
        mongoac_read_preference_set_mode(read_preference, mode);

        CHECK(mongoac_read_preference_get_tag_sets_len(read_preference) == 0u);
    }
}

TEST_CASE("set_max_staleness_from_secs", "[mongoac][read_preference]") {
    SECTION("null") {
        mongoac_read_preference_set_max_staleness_from_secs(nullptr, 123);
        SUCCEED();
    }

    SECTION("basic") {
        auto const v = GENERATE(from_range(mode_cases));

        CAPTURE(v);

        auto const read_preference_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
        auto const read_preference = read_preference_owner.get();

        mongoac_read_preference_set_mode(read_preference, v);
        mongoac_read_preference_set_max_staleness_from_secs(read_preference, 123);

        if (v == MONGOAC_READ_PREFERENCE_MODE_PRIMARY) {
            CHECK(mongoac_read_preference_get_max_staleness_is_none(read_preference));
            CHECK(
                mongoac_read_preference_get_max_staleness_as_secs(read_preference) ==
                std::uint64_t{}); // Default::default()
        } else {
            CHECK_FALSE(mongoac_read_preference_get_max_staleness_is_none(read_preference));
            CHECK(mongoac_read_preference_get_max_staleness_as_secs(read_preference) == 123);
        }
    }
}

TEST_CASE("set_max_staleness_none", "[mongoac][read_preference]") {
    SECTION("null") {
        mongoac_read_preference_set_max_staleness_none(nullptr);
        SUCCEED();
    }

    SECTION("clears") {
        auto const read_preference_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
        auto const read_preference = read_preference_owner.get();

        mongoac_read_preference_set_mode(read_preference, MONGOAC_READ_PREFERENCE_MODE_SECONDARY);
        mongoac_read_preference_set_max_staleness_from_secs(read_preference, 123);

        CHECK_FALSE(mongoac_read_preference_get_max_staleness_is_none(read_preference));
        CHECK(mongoac_read_preference_get_max_staleness_as_secs(read_preference) == 123);

        mongoac_read_preference_set_max_staleness_none(read_preference);

        CHECK(mongoac_read_preference_get_max_staleness_is_none(read_preference));
        CHECK(
            mongoac_read_preference_get_max_staleness_as_secs(read_preference) ==
            std::uint64_t{}); // Default::default()
    }
}

TEST_CASE("set_mode", "[mongoac][read_preference]") {
    auto const read_preference_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
    auto const read_preference = read_preference_owner.get();

    SECTION("null") {
        mongoac_read_preference_set_mode(nullptr, MONGOAC_READ_PREFERENCE_MODE_PRIMARY);
        SUCCEED();
    }

    SECTION("fields") {
        auto const source_mode = GENERATE(from_range(mode_cases));
        auto const target_mode = GENERATE(from_range(mode_cases));

        CAPTURE(source_mode);
        CAPTURE(target_mode);

        mongoac_read_preference_set_mode(read_preference, source_mode);
        mongoac_read_preference_set_max_staleness_from_secs(read_preference, 123);

        mongoac_read_preference_set_mode(read_preference, target_mode);

        CHECK(mongoac_read_preference_get_mode(read_preference) == target_mode);

        if (source_mode == MONGOAC_READ_PREFERENCE_MODE_PRIMARY ||
            target_mode == MONGOAC_READ_PREFERENCE_MODE_PRIMARY) {
            CHECK(mongoac_read_preference_get_max_staleness_is_none(read_preference));
        } else {
            CHECK(mongoac_read_preference_get_max_staleness_as_secs(read_preference) == 123);
        }
    }

    SECTION("unknown") {
        mongoac_read_preference_set_mode(read_preference, MONGOAC_READ_PREFERENCE_MODE_SECONDARY);
        mongoac_read_preference_set_max_staleness_from_secs(read_preference, 123);

        mongoac_read_preference_set_mode(read_preference, MONGOAC_READ_PREFERENCE_MODE_UNKNOWN);

        CHECK(mongoac_read_preference_get_mode(read_preference) == MONGOAC_READ_PREFERENCE_MODE_SECONDARY);
        CHECK(mongoac_read_preference_get_max_staleness_as_secs(read_preference) == 123);
    }
}

TEST_CASE("set_tag_sets_none", "[mongoac][read_preference]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const read_preference_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
    auto const read_preference = read_preference_owner.get();

    REQUIRE(mongoac_read_preference_get_mode(read_preference) == MONGOAC_READ_PREFERENCE_MODE_PRIMARY);

    SECTION("null") {
        mongoac_read_preference_set_tag_sets_none(nullptr);
        SUCCEED();
    }

    SECTION("primary") {
        mongoac_read_preference_set_tag_sets_none(read_preference);
        CHECK(mongoac_read_preference_get_tag_sets_len(read_preference) == 0u);
    }

    SECTION("basic") {
        auto const mode = GENERATE(from_range(std::begin(mode_cases) + 1u, std::end(mode_cases))); // Exclude "primary".
        CAPTURE(mode);
        REQUIRE(mode != MONGOAC_READ_PREFERENCE_MODE_PRIMARY);
        mongoac_read_preference_set_mode(read_preference, mode);

        mongoac_read_preference_set_tag_sets_none(read_preference);
        CHECK(mongoac_read_preference_get_tag_sets_len(read_preference) == 0u);

        {
            mongoac_read_preference_set_tag_sets_push(read_preference, {}, {}, error);
            REQUIRE_MONGOAC_ERROR_OK(error);
            REQUIRE(mongoac_read_preference_get_tag_sets_len(read_preference) == 1u);
        }

        mongoac_read_preference_set_tag_sets_none(read_preference);
        CHECK(mongoac_read_preference_get_tag_sets_len(read_preference) == 0u);
    }
}

TEST_CASE("set_tag_sets_push", "[mongoac][read_preference]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const read_preference_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
    auto const read_preference = read_preference_owner.get();

    REQUIRE(mongoac_read_preference_get_mode(read_preference) == MONGOAC_READ_PREFERENCE_MODE_PRIMARY);

    SECTION("null read preference") {
        mongoac_read_preference_set_tag_sets_push(nullptr, {}, {}, error);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
    }

    SECTION("null key") {
        auto const mode = GENERATE(from_range(std::begin(mode_cases), std::end(mode_cases)));
        CAPTURE(mode);
        mongoac_read_preference_set_mode(read_preference, mode);

        auto const [msg, keys] = GENERATE(
            table<char const*, std::array<mongoac_string_view_t, 3u>>({
                {"keys element at index 0: must not be null", to_array({{}, "b"_sv, "c"_sv})},
                {"keys element at index 1: must not be null", to_array({"a"_sv, {}, "c"_sv})},
                {"keys element at index 2: must not be null", to_array({"a"_sv, "b"_sv, {}})},
            }));

        mongoac_read_preference_set_tag_sets_push(read_preference, {keys.data(), keys.size()}, {}, error);

        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, msg);
        CHECK(mongoac_read_preference_get_tag_sets_len(read_preference) == 0u);
    }

    SECTION("null value") {
        auto const mode = GENERATE(from_range(std::begin(mode_cases), std::end(mode_cases)));
        CAPTURE(mode);
        mongoac_read_preference_set_mode(read_preference, mode);

        auto const [msg, values] = GENERATE(
            table<char const*, std::array<mongoac_string_view_t, 3u>>({
                {"values element at index 0: must not be null", to_array({{}, "b"_sv, "c"_sv})},
                {"values element at index 1: must not be null", to_array({"a"_sv, {}, "c"_sv})},
                {"values element at index 2: must not be null", to_array({"a"_sv, "b"_sv, {}})},
            }));

        mongoac_read_preference_set_tag_sets_push(read_preference, {}, {values.data(), values.size()}, error);

        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, msg);
        CHECK(mongoac_read_preference_get_tag_sets_len(read_preference) == 0u);
    }

    SECTION("invalid key") {
        auto const mode = GENERATE(from_range(std::begin(mode_cases), std::end(mode_cases)));
        CAPTURE(mode);
        mongoac_read_preference_set_mode(read_preference, mode);

        auto const [msg, keys] = GENERATE(
            table<char const*, std::array<mongoac_string_view_t, 3u>>({
                {"keys element at index 0: invalid UTF-8", to_array({"\xff"_sv, "b"_sv, "c"_sv})},
                {"keys element at index 1: invalid UTF-8", to_array({"a"_sv, "\xff"_sv, "c"_sv})},
                {"keys element at index 2: invalid UTF-8", to_array({"a"_sv, "b"_sv, "\xff"_sv})},
            }));

        mongoac_read_preference_set_tag_sets_push(read_preference, {keys.data(), keys.size()}, {}, error);

        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, msg);
        CHECK(mongoac_read_preference_get_tag_sets_len(read_preference) == 0u);
    }

    SECTION("invalid value") {
        auto const mode = GENERATE(from_range(std::begin(mode_cases), std::end(mode_cases)));
        CAPTURE(mode);
        mongoac_read_preference_set_mode(read_preference, mode);

        auto const [msg, values] = GENERATE(
            table<char const*, std::array<mongoac_string_view_t, 3u>>({
                {"values element at index 0: invalid UTF-8", to_array({"\xff"_sv, "b"_sv, "c"_sv})},
                {"values element at index 1: invalid UTF-8", to_array({"a"_sv, "\xff"_sv, "c"_sv})},
                {"values element at index 2: invalid UTF-8", to_array({"a"_sv, "b"_sv, "\xff"_sv})},
            }));

        mongoac_read_preference_set_tag_sets_push(read_preference, {}, {values.data(), values.size()}, error);

        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, msg);
        CHECK(mongoac_read_preference_get_tag_sets_len(read_preference) == 0u);
    }

    SECTION("length mismatch") {
        auto const mode = GENERATE(from_range(std::begin(mode_cases), std::end(mode_cases)));
        CAPTURE(mode);
        mongoac_read_preference_set_mode(read_preference, mode);

        static constexpr auto const keys = to_array<mongoac_string_view_t>({"k1"_sv, "k2"_sv});
        static constexpr auto const values = to_array<mongoac_string_view_t>({"v1"_sv, "v2"_sv});

        auto const [msg, k, v] = GENERATE(
            table<char const*, mongoac_string_view_array_t, mongoac_string_view_array_t>({
                {"length mismatch: 1 != 0", {keys.data(), 1u}, {}},
                {"length mismatch: 0 != 1", {}, {values.data(), 1u}},
                {"length mismatch: 0 != 1", {keys.data(), 0u}, {values.data(), 1u}},
                {"length mismatch: 1 != 0", {keys.data(), 1u}, {values.data(), 0u}},
                {"length mismatch: 1 != 2", {keys.data(), 1u}, {values.data(), 2u}},
                {"length mismatch: 2 != 1", {keys.data(), 2u}, {values.data(), 1u}},
            }));

        mongoac_read_preference_set_tag_sets_push(read_preference, k, v, error);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, msg);
        CHECK(mongoac_read_preference_get_tag_sets_len(read_preference) == 0u);
    }

    SECTION("primary") {
        mongoac_read_preference_set_tag_sets_push(read_preference, {}, {}, error); // Do nothing.
        CHECK_MONGOAC_ERROR_OK(error);
        CHECK(mongoac_read_preference_get_tag_sets_len(read_preference) == 0u);
    }

    SECTION("basic") {
        auto const mode = GENERATE(from_range(std::begin(mode_cases) + 1u, std::end(mode_cases))); // Exclude "primary".
        CAPTURE(mode);
        REQUIRE(mode != MONGOAC_READ_PREFERENCE_MODE_PRIMARY);
        mongoac_read_preference_set_mode(read_preference, mode);

        auto const keys = to_array({"key1"_sv, "key2"_sv, "key3"_sv});
        auto const values = to_array({"val1"_sv, "val2"_sv, "val3"_sv});

        mongoac_read_preference_set_tag_sets_push(
            read_preference, {keys.data(), keys.size()}, {values.data(), values.size()}, error);
        CHECK_MONGOAC_ERROR_OK(error);

        CHECK(mongoac_read_preference_get_tag_sets_len(read_preference) == 1u);
        CHECK(mongoac_read_preference_get_tag_sets_get_len(read_preference, 0u) == keys.size());

        // ["key1", "key2", "key3"] (arbitrary order)
        {
            auto const actual_keys_owner =
                make_unique(mongoac_read_preference_get_tag_sets_get_keys(read_preference, 0u));

            std::set<std::string_view> expected_keys;
            std::set<std::string_view> actual_keys;

            auto const op = [](auto v) { return from_mongoac(v); };
            std::transform(std::begin(keys), std::end(keys), std::inserter(expected_keys, std::end(expected_keys)), op);
            std::transform(
                std::begin(actual_keys_owner),
                std::end(actual_keys_owner),
                std::inserter(actual_keys, std::end(actual_keys)),
                op);

            CHECK(actual_keys == expected_keys);
        }

        // {"key1": "val1", "key2": "val2", "key3": "val3"} (arbitrary order)
        for (std::size_t i = 0; i < keys.size(); ++i) {
            auto const key = keys[i];

            auto const expected_value = from_mongoac(values[i]);
            auto const actual_value =
                from_mongoac(mongoac_read_preference_get_tag_sets_get_get(read_preference, 0, key));

            CHECK(actual_value == expected_value);
        }

        mongoac_read_preference_set_tag_sets_push(read_preference, {}, {}, error);
        CHECK_MONGOAC_ERROR_OK(error);
        CHECK(mongoac_read_preference_get_tag_sets_len(read_preference) == 2u);

        // {"key1": "val1", "key2": "val2", "key3": "val3"} (same as before)
        {
            CHECK(mongoac_read_preference_get_tag_sets_get_len(read_preference, 0u) == keys.size());

            for (std::size_t i = 0; i < keys.size(); ++i) {
                auto const key = keys[i];
                auto const value = values[i];

                CHECK(
                    from_mongoac(mongoac_read_preference_get_tag_sets_get_get(read_preference, 0, key)) ==
                    from_mongoac(value));
            }
        }

        // {} (empty)
        {
            CHECK(mongoac_read_preference_get_tag_sets_get_len(read_preference, 1u) == 0u);

            for (auto const key : keys) {
                CAPTURE(key);
                auto const value = from_mongoac(mongoac_read_preference_get_tag_sets_get_get(read_preference, 1u, key));
                CHECK(value == std::string_view{}); // Default::default()
            }
        }
    }
}
