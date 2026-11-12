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

#include <mongoac/run_cursor_command_options.h>

//

#include <mongoac/test/array.hh>
#include <mongoac/test/bson.hh>
#include <mongoac/test/error.hh>
#include <mongoac/test/memory.hh>
#include <mongoac/test/string.hh>

#include <mongoac/cursor_type.h>
#include <mongoac/error.h>
#include <mongoac/read_preference.h>

#include <bson/bson.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <cstdint>
#include <string_view>

using mongoac::test::from_mongoac;
using mongoac::test::make_unique;
using mongoac::test::operator""_bson;
using mongoac::test::operator""_sv;
using mongoac::test::to_array;
using mongoac::test::to_bson;
using mongoac::test::to_mongoac;

TEST_CASE("destroy", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        mongoac_run_cursor_command_options_destroy(nullptr);
        SUCCEED();
    }
}

TEST_CASE("clone", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        auto const options =
            make_unique(mongoac_run_cursor_command_options_clone(nullptr), &mongoac_run_cursor_command_options_destroy);

        CHECK(options == nullptr);
    }

    SECTION("basic") {
        auto const source_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const source = source_owner.get();

        mongoac_run_cursor_command_options_set_batch_size(source, 123);
        mongoac_run_cursor_command_options_set_comment_string(source, "abc"_sv);
        mongoac_run_cursor_command_options_set_cursor_type(source, MONGOAC_CURSOR_TYPE_TAILABLE);
        mongoac_run_cursor_command_options_set_max_time_from_secs(source, 456);

        auto const target_owner =
            make_unique(mongoac_run_cursor_command_options_clone(source), &mongoac_run_cursor_command_options_destroy);
        auto const target = target_owner.get();

        CHECK(target != nullptr);
        CHECK(target != source);

        CHECK(mongoac_run_cursor_command_options_get_batch_size(target) == 123);
        CHECK(from_mongoac(mongoac_run_cursor_command_options_get_comment_string(target)) == "abc");
        CHECK(mongoac_run_cursor_command_options_get_cursor_type(target) == MONGOAC_CURSOR_TYPE_TAILABLE);
        CHECK(mongoac_run_cursor_command_options_get_max_time_as_secs(target) == 456);
    }
}

TEST_CASE("new", "[mongoac][run_cursor_command_options]") {
    SECTION("default") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        CHECK(mongoac_run_cursor_command_options_get_batch_size_is_none(options));
        CHECK(mongoac_run_cursor_command_options_get_comment_is_none(options));
        CHECK(mongoac_run_cursor_command_options_get_cursor_type_is_none(options));
        CHECK(mongoac_run_cursor_command_options_get_max_time_is_none(options));
        CHECK(mongoac_run_cursor_command_options_get_read_preference(options) == nullptr);
    }
}

TEST_CASE("get_batch_size_is_none", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        CHECK(mongoac_run_cursor_command_options_get_batch_size_is_none(nullptr));
    }

    SECTION("none") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        CHECK(mongoac_run_cursor_command_options_get_batch_size_is_none(options));
    }
}

TEST_CASE("get_batch_size", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        CHECK(mongoac_run_cursor_command_options_get_batch_size(nullptr) == std::uint32_t{}); // Default::default()
    }

    SECTION("none") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        CHECK(mongoac_run_cursor_command_options_get_batch_size(options) == std::uint32_t{}); // Default::default()
    }
}

TEST_CASE("get_comment", "[mongoac][run_cursor_command_options]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();

    SECTION("null") {
        auto const comment = from_mongoac(mongoac_run_cursor_command_options_get_comment(nullptr, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK(comment == nullptr);
    }

    SECTION("none") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        auto const comment = from_mongoac(mongoac_run_cursor_command_options_get_comment(options, error));
        CHECK_MONGOAC_ERROR_OK(error);
        CHECK(comment == nullptr);
    }
}

TEST_CASE("get_comment_string", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        CHECK(mongoac_run_cursor_command_options_get_comment_string(nullptr).ptr == nullptr);
    }

    SECTION("none") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        CHECK(mongoac_run_cursor_command_options_get_comment_string(options).ptr == nullptr);
    }

    SECTION("type mismatch: string") {
        auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
        auto const error = error_owner.get();
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        auto const comment_owner = REQUIRE_MAKE_UNIQUE(R"({"v": 123})"_bson, &bson_destroy);
        mongoac_run_cursor_command_options_set_comment(options, to_mongoac(comment_owner.get()), error);
        REQUIRE_MONGOAC_ERROR_OK(error);

        CHECK(mongoac_run_cursor_command_options_get_comment_string(options).ptr == nullptr);
    }
}

TEST_CASE("get_comment_is_none", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        CHECK(mongoac_run_cursor_command_options_get_comment_is_none(nullptr));
    }

    SECTION("none") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        CHECK(mongoac_run_cursor_command_options_get_comment_is_none(options));
    }
}

TEST_CASE("get_cursor_type_is_none", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        CHECK(mongoac_run_cursor_command_options_get_cursor_type_is_none(nullptr));
    }

    SECTION("none") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        CHECK(mongoac_run_cursor_command_options_get_cursor_type_is_none(options));
    }
}

TEST_CASE("get_cursor_type", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        CHECK(mongoac_run_cursor_command_options_get_cursor_type(nullptr) == MONGOAC_CURSOR_TYPE_UNKNOWN);
    }

    SECTION("none") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        CHECK(mongoac_run_cursor_command_options_get_cursor_type(options) == MONGOAC_CURSOR_TYPE_UNKNOWN);
    }
}

TEST_CASE("get_max_time_as_secs", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        CHECK(
            mongoac_run_cursor_command_options_get_max_time_as_secs(nullptr) == std::uint64_t{}); // Default::default()
    }

    SECTION("none") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        CHECK(
            mongoac_run_cursor_command_options_get_max_time_as_secs(options) == std::uint64_t{}); // Default::default()
    }
}

TEST_CASE("get_max_time_subsec_millis", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        CHECK(
            mongoac_run_cursor_command_options_get_max_time_subsec_millis(nullptr) ==
            std::uint64_t{}); // Default::default()
    }

    SECTION("none") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        CHECK(
            mongoac_run_cursor_command_options_get_max_time_subsec_millis(options) ==
            std::uint64_t{}); // Default::default()
    }
}

TEST_CASE("get_max_time_is_none", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        CHECK(mongoac_run_cursor_command_options_get_max_time_is_none(nullptr));
    }

    SECTION("none") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        CHECK(mongoac_run_cursor_command_options_get_max_time_is_none(options));
    }
}

TEST_CASE("get_read_preference", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        CHECK(mongoac_run_cursor_command_options_get_read_preference(nullptr) == nullptr);
    }

    SECTION("none") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        CHECK(mongoac_run_cursor_command_options_get_read_preference(options) == nullptr);
    }
}

TEST_CASE("set_batch_size", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        mongoac_run_cursor_command_options_set_batch_size(nullptr, 123);
        SUCCEED();
    }

    SECTION("basic") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        mongoac_run_cursor_command_options_set_batch_size(options, 123);
        CHECK(mongoac_run_cursor_command_options_get_batch_size(options) == 123);

        mongoac_run_cursor_command_options_set_batch_size(options, 456);
        CHECK(mongoac_run_cursor_command_options_get_batch_size(options) == 456);
    }
}

TEST_CASE("set_batch_size_none", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        mongoac_run_cursor_command_options_set_batch_size_none(nullptr);
        SUCCEED();
    }

    SECTION("basic") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        mongoac_run_cursor_command_options_set_batch_size(options, 123);
        CHECK(mongoac_run_cursor_command_options_get_batch_size(options) == 123);

        mongoac_run_cursor_command_options_set_batch_size_none(options);
        CHECK(mongoac_run_cursor_command_options_get_batch_size_is_none(options));
        CHECK(mongoac_run_cursor_command_options_get_batch_size(options) == std::uint32_t{}); // Default::default()

        mongoac_run_cursor_command_options_set_batch_size(options, 456);
        CHECK(mongoac_run_cursor_command_options_get_batch_size(options) == 456);

        mongoac_run_cursor_command_options_set_batch_size_none(options);
        CHECK(mongoac_run_cursor_command_options_get_batch_size_is_none(options));
        CHECK(mongoac_run_cursor_command_options_get_batch_size(options) == std::uint32_t{}); // Default::default()
    }
}

TEST_CASE("set_comment", "[mongoac][run_cursor_command_options]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const options_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
    auto const options = options_owner.get();

    mongoac_run_cursor_command_options_set_comment(
        options, to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"v": "abc"})"_bson, &bson_destroy).get()), error);
    REQUIRE_MONGOAC_ERROR_OK(error);
    REQUIRE_FALSE(mongoac_run_cursor_command_options_get_comment_is_none(options));
    REQUIRE(from_mongoac(mongoac_run_cursor_command_options_get_comment_string(options)) == "abc");

    SECTION("null options") {
        auto const comment_owner = REQUIRE_MAKE_UNIQUE(R"({"v": "abc"})"_bson, &bson_destroy);
        mongoac_run_cursor_command_options_set_comment(nullptr, to_mongoac(comment_owner.get()), error);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
    }

    SECTION("null value") {
        mongoac_run_cursor_command_options_set_comment(options, {}, error);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);

        CHECK(from_mongoac(mongoac_run_cursor_command_options_get_comment_string(options)) == "abc");
    }

    SECTION("missing field") {
        auto const comment_owner = REQUIRE_MAKE_UNIQUE(R"({"detail": "x"})"_bson, &bson_destroy);
        mongoac_run_cursor_command_options_set_comment(options, to_mongoac(comment_owner.get()), error);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "missing field 'v'");

        CHECK(from_mongoac(mongoac_run_cursor_command_options_get_comment_string(options)) == "abc");
    }

    SECTION("invalid BSON") {
        std::uint8_t const bytes = {};
        mongoac_run_cursor_command_options_set_comment(options, {&bytes, 1u}, error);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);

        CHECK(from_mongoac(mongoac_run_cursor_command_options_get_comment_string(options)) == "abc");
    }

    SECTION("basic") {
        mongoac_run_cursor_command_options_set_comment(
            options, to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"v": 123})"_bson, &bson_destroy).get()), error);

        auto const v = from_mongoac(mongoac_run_cursor_command_options_get_comment(options, error));
        CHECK_MONGOAC_ERROR_OK(error);
        auto const doc = REQUIRE_BSON_STATIC(v);
        auto const value = REQUIRE_BSON_ITER_FIND(doc, "v");
        CHECK(bson_iter_int32(&value) == 123);
    }
}

TEST_CASE("set_comment_none", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        mongoac_run_cursor_command_options_set_comment_none(nullptr);
        SUCCEED();
    }

    SECTION("basic") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        mongoac_run_cursor_command_options_set_comment_string(options, "abc"_sv);
        REQUIRE(from_mongoac(mongoac_run_cursor_command_options_get_comment_string(options)) == "abc");

        mongoac_run_cursor_command_options_set_comment_none(options);
        CHECK(mongoac_run_cursor_command_options_get_comment_is_none(options));
        CHECK(mongoac_run_cursor_command_options_get_comment_string(options).ptr == nullptr);

        mongoac_run_cursor_command_options_set_comment_string(options, "def"_sv);
        CHECK(from_mongoac(mongoac_run_cursor_command_options_get_comment_string(options)) == "def");

        mongoac_run_cursor_command_options_set_comment_none(options);
        CHECK(mongoac_run_cursor_command_options_get_comment_is_none(options));
        CHECK(mongoac_run_cursor_command_options_get_comment_string(options).ptr == nullptr);
    }
}

TEST_CASE("set_comment_string", "[mongoac][run_cursor_command_options]") {
    auto const options_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
    auto const options = options_owner.get();

    mongoac_run_cursor_command_options_set_comment_string(options, "abc"_sv);
    REQUIRE_FALSE(mongoac_run_cursor_command_options_get_comment_is_none(options));
    REQUIRE(from_mongoac(mongoac_run_cursor_command_options_get_comment_string(options)) == "abc");

    SECTION("null options") {
        mongoac_run_cursor_command_options_set_comment_string(nullptr, "abc"_sv);
        SUCCEED();
    }

    SECTION("null value") {
        mongoac_run_cursor_command_options_set_comment_string(options, {});
        CHECK(from_mongoac(mongoac_run_cursor_command_options_get_comment_string(options)) == "abc");
    }

    SECTION("type mismatch") {
        auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
        auto const error = error_owner.get();

        mongoac_run_cursor_command_options_set_comment(
            options, to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"v": 123})"_bson, &bson_destroy).get()), error);
        REQUIRE_MONGOAC_ERROR_OK(error);

        CHECK(mongoac_run_cursor_command_options_get_comment_string(options).ptr == nullptr);
    }

    SECTION("basic") {
        mongoac_run_cursor_command_options_set_comment_string(options, "def"_sv);
        CHECK(from_mongoac(mongoac_run_cursor_command_options_get_comment_string(options)) == "def");
    }
}

namespace {

constexpr auto cursor_type_cases = to_array<mongoac_cursor_type_t>({
    MONGOAC_CURSOR_TYPE_NON_TAILABLE,
    MONGOAC_CURSOR_TYPE_TAILABLE,
    MONGOAC_CURSOR_TYPE_TAILABLE_AWAIT,
});

} // namespace

TEST_CASE("set_cursor_type", "[mongoac][run_cursor_command_options]") {
    auto const options_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
    auto const options = options_owner.get();

    SECTION("null") {
        mongoac_run_cursor_command_options_set_cursor_type(nullptr, MONGOAC_CURSOR_TYPE_NON_TAILABLE);
        SUCCEED();
    }

    SECTION("fields") {
        auto const v = GENERATE(from_range(cursor_type_cases));

        mongoac_run_cursor_command_options_set_cursor_type(options, v);
        CHECK(mongoac_run_cursor_command_options_get_cursor_type(options) == v);
    }

    SECTION("unknown") {
        mongoac_run_cursor_command_options_set_cursor_type(options, MONGOAC_CURSOR_TYPE_NON_TAILABLE);

        auto const v = GENERATE(
            values<mongoac_cursor_type_t>({
                MONGOAC_CURSOR_TYPE_UNKNOWN, // INT32_MIN
                -1,
                cursor_type_cases.back() + 1,
                INT32_MAX,
            }));

        mongoac_run_cursor_command_options_set_cursor_type(options, v);

        CHECK(mongoac_run_cursor_command_options_get_cursor_type(options) == MONGOAC_CURSOR_TYPE_NON_TAILABLE);
    }
}

TEST_CASE("set_cursor_type_none", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        mongoac_run_cursor_command_options_set_cursor_type_none(nullptr);
        SUCCEED();
    }

    SECTION("basic") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        mongoac_run_cursor_command_options_set_cursor_type(options, MONGOAC_CURSOR_TYPE_TAILABLE);
        CHECK_FALSE(mongoac_run_cursor_command_options_get_cursor_type_is_none(options));
        CHECK(mongoac_run_cursor_command_options_get_cursor_type(options) == MONGOAC_CURSOR_TYPE_TAILABLE);

        mongoac_run_cursor_command_options_set_cursor_type_none(options);
        CHECK(mongoac_run_cursor_command_options_get_cursor_type_is_none(options));
        CHECK(mongoac_run_cursor_command_options_get_cursor_type(options) == MONGOAC_CURSOR_TYPE_UNKNOWN);

        mongoac_run_cursor_command_options_set_cursor_type(options, MONGOAC_CURSOR_TYPE_TAILABLE_AWAIT);
        CHECK_FALSE(mongoac_run_cursor_command_options_get_cursor_type_is_none(options));
        CHECK(mongoac_run_cursor_command_options_get_cursor_type(options) == MONGOAC_CURSOR_TYPE_TAILABLE_AWAIT);

        mongoac_run_cursor_command_options_set_cursor_type_none(options);
        CHECK(mongoac_run_cursor_command_options_get_cursor_type_is_none(options));
        CHECK(mongoac_run_cursor_command_options_get_cursor_type(options) == MONGOAC_CURSOR_TYPE_UNKNOWN);
    }
}

TEST_CASE("set_max_time_from_millis", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        mongoac_run_cursor_command_options_set_max_time_from_millis(nullptr, 123456u);
        SUCCEED();
    }

    SECTION("basic") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        mongoac_run_cursor_command_options_set_max_time_from_millis(options, 123456u);

        CHECK_FALSE(mongoac_run_cursor_command_options_get_max_time_is_none(options));
        CHECK(mongoac_run_cursor_command_options_get_max_time_as_secs(options) == 123u);
        CHECK(mongoac_run_cursor_command_options_get_max_time_subsec_millis(options) == 456u);
    }
}

TEST_CASE("set_max_time_from_secs", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        mongoac_run_cursor_command_options_set_max_time_from_secs(nullptr, 123u);
        SUCCEED();
    }

    SECTION("basic") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        mongoac_run_cursor_command_options_set_max_time_from_secs(options, 123u);

        CHECK_FALSE(mongoac_run_cursor_command_options_get_max_time_is_none(options));
        CHECK(mongoac_run_cursor_command_options_get_max_time_as_secs(options) == 123u);
        CHECK(mongoac_run_cursor_command_options_get_max_time_subsec_millis(options) == 0u);
    }
}

TEST_CASE("set_max_time_none", "[mongoac][run_cursor_command_options]") {
    SECTION("null") {
        mongoac_run_cursor_command_options_set_max_time_none(nullptr);
        SUCCEED();
    }

    SECTION("basic") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
        auto const options = options_owner.get();

        mongoac_run_cursor_command_options_set_max_time_from_secs(options, 123u);
        REQUIRE_FALSE(mongoac_run_cursor_command_options_get_max_time_is_none(options));
        REQUIRE(mongoac_run_cursor_command_options_get_max_time_as_secs(options) == 123u);

        mongoac_run_cursor_command_options_set_max_time_none(options);
        CHECK(mongoac_run_cursor_command_options_get_max_time_is_none(options));
        CHECK(
            mongoac_run_cursor_command_options_get_max_time_as_secs(options) == std::uint64_t{}); // Default::default()

        mongoac_run_cursor_command_options_set_max_time_from_secs(options, 456u);
        REQUIRE_FALSE(mongoac_run_cursor_command_options_get_max_time_is_none(options));
        REQUIRE(mongoac_run_cursor_command_options_get_max_time_as_secs(options) == 456u);

        mongoac_run_cursor_command_options_set_max_time_none(options);
        CHECK(mongoac_run_cursor_command_options_get_max_time_is_none(options));
        CHECK(
            mongoac_run_cursor_command_options_get_max_time_as_secs(options) == std::uint64_t{}); // Default::default()
    }
}

TEST_CASE("set_read_preference", "[mongoac][run_cursor_command_options]") {
    auto const options_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_run_cursor_command_options_new(), &mongoac_run_cursor_command_options_destroy);
    auto const options = options_owner.get();

    {
        mongoac_run_cursor_command_options_set_read_preference(
            options, REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy).get());
        auto const v = mongoac_run_cursor_command_options_get_read_preference(options);
        REQUIRE(v != nullptr);
        REQUIRE(mongoac_read_preference_get_mode(v) == MONGOAC_READ_PREFERENCE_MODE_PRIMARY);
    }

    SECTION("null options") {
        mongoac_run_cursor_command_options_set_read_preference(
            nullptr, REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy).get());
    }

    SECTION("null value") {
        mongoac_run_cursor_command_options_set_read_preference(options, nullptr);

        CHECK(mongoac_run_cursor_command_options_get_read_preference(options) == nullptr);
    }

    SECTION("basic") {
        {
            auto const v = REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
            mongoac_read_preference_set_mode(v.get(), MONGOAC_READ_PREFERENCE_MODE_SECONDARY);
            mongoac_run_cursor_command_options_set_read_preference(options, v.get());
        }

        auto const v = mongoac_run_cursor_command_options_get_read_preference(options);
        CHECK(mongoac_read_preference_get_mode(v) == MONGOAC_READ_PREFERENCE_MODE_SECONDARY);
    }
}
