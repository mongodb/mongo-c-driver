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

#include <mongoac/error.h>

//

#include <cstdint>
#include <memory>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <mongoac/test/memory.hh>
#include <mongoac/test/string.hh>

using mongoac::test::from_mongoac;
using mongoac::test::make_unique;

TEST_CASE("default", "[mongoac][error]") {
    auto const error_owner = make_unique(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();

    CHECK(error != nullptr);
    CHECK(mongoac_error_category(error) == MONGOAC_ERROR_CATEGORY_NONE);
    CHECK(mongoac_error_code(error) == MONGOAC_ERROR_CODE_OK);
}

TEST_CASE("clone", "[mongoac][error]") {
    SECTION("null") {
        auto const copy_owner = make_unique(mongoac_error_clone(nullptr), &mongoac_error_destroy);
        auto const copy = copy_owner.get();

        CHECK(copy == nullptr);
    }

    SECTION("default") {
        auto const error_owner = make_unique(mongoac_error_new(), &mongoac_error_destroy);
        auto const error = error_owner.get();

        auto const copy_owner = make_unique(mongoac_error_clone(error), &mongoac_error_destroy);
        auto const copy = copy_owner.get();

        CHECK(copy != nullptr);
        CHECK(mongoac_error_category(copy) == mongoac_error_category(error));
        CHECK(mongoac_error_code(copy) == mongoac_error_code(error));
    }

    SECTION("custom") {
        auto const error_owner = make_unique(mongoac_error_new(), &mongoac_error_destroy);
        auto const error = error_owner.get();

        auto const category = GENERATE(
            values<std::int32_t>({
                INT32_MIN,
                INT32_MIN + 1,
                -1,
                0,
                1,
                INT32_MAX - 1,
                INT32_MAX,
            }));

        auto const code = GENERATE(
            values<std::int32_t>({
                INT32_MIN,
                INT32_MIN + 1,
                -1,
                0,
                1,
                INT32_MAX - 1,
                INT32_MAX,
            }));

        CHECK_NOTHROW(mongoac_error_set(error, category, code));

        auto const copy_owner = make_unique(mongoac_error_clone(error), &mongoac_error_destroy);
        auto const copy = copy_owner.get();

        CHECK(copy != nullptr);
        CHECK(mongoac_error_category(copy) == mongoac_error_category(error));
        CHECK(mongoac_error_code(copy) == mongoac_error_code(error));
    }
}

TEST_CASE("category", "[mongoac][error]") {
    SECTION("null") {
        CHECK(mongoac_error_category(nullptr) == MONGOAC_ERROR_CATEGORY_NONE);
    }
}

TEST_CASE("code", "[mongoac][error]") {
    SECTION("null") {
        CHECK(mongoac_error_code(nullptr) == MONGOAC_ERROR_CODE_OK);
    }
}

TEST_CASE("message", "[mongoac][error]") {
    SECTION("null") {
        CHECK(from_mongoac(mongoac_error_message(nullptr)) == "");
    }

    SECTION("default") {
        auto const error_owner = make_unique(mongoac_error_new(), &mongoac_error_destroy);
        auto const error = error_owner.get();

        SECTION("default") {
            CHECK(from_mongoac(mongoac_error_message(error)) == "");
        }
    }

    SECTION("mongoac") {
        auto const error_owner = make_unique(mongoac_error_new(), &mongoac_error_destroy);
        auto const error = error_owner.get();

        SECTION("ok") {
            CHECK_NOTHROW(mongoac_error_set(error, MONGOAC_ERROR_CATEGORY_MONGOAC, MONGOAC_ERROR_CODE_OK));
            CHECK(from_mongoac(mongoac_error_message(error)) == "ok");
        }

        SECTION("invalid argument") {
            CHECK_NOTHROW(
                mongoac_error_set(error, MONGOAC_ERROR_CATEGORY_MONGOAC, MONGOAC_ERROR_CODE_INVALID_ARGUMENT));
            CHECK(from_mongoac(mongoac_error_message(error)) == "invalid argument");
        }

        SECTION("runtime error") {
            CHECK_NOTHROW(mongoac_error_set(error, MONGOAC_ERROR_CATEGORY_MONGOAC, MONGOAC_ERROR_CODE_RUNTIME_ERROR));
            CHECK(from_mongoac(mongoac_error_message(error)) == "runtime error");
        }

        SECTION("unknown error code") {
            CHECK_NOTHROW(mongoac_error_set(error, MONGOAC_ERROR_CATEGORY_MONGOAC, -1));
            CHECK(from_mongoac(mongoac_error_message(error)) == "unknown error code");
        }
    }

    SECTION("unknown") {
        auto const error_owner = make_unique(mongoac_error_new(), &mongoac_error_destroy);
        auto const error = error_owner.get();

        auto const code = GENERATE(
            values<std::int32_t>({
                INT32_MIN,
                INT32_MIN + 1,
                -1,
                0,
                1,
                INT32_MAX - 1,
                INT32_MAX,
            }));

        CHECK_NOTHROW(mongoac_error_set(error, MONGOAC_ERROR_CATEGORY_UNKNOWN, code));
        CHECK(from_mongoac(mongoac_error_message(error)) == "");
    }
}

TEST_CASE("clear", "[mongoac][error]") {
    SECTION("null") {
        CHECK_NOTHROW(mongoac_error_clear(nullptr));
    }

    SECTION("default") {
        auto const error_owner = make_unique(mongoac_error_new(), &mongoac_error_destroy);
        auto const error = error_owner.get();

        CHECK_NOTHROW(mongoac_error_clear(error));

        CHECK(mongoac_error_category(error) == MONGOAC_ERROR_CATEGORY_NONE);
        CHECK(mongoac_error_code(error) == MONGOAC_ERROR_CODE_OK);
    }
}

TEST_CASE("set", "[mongoac][error]") {
    auto const error_owner = make_unique(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();

    SECTION("null") {
        CHECK_NOTHROW(mongoac_error_set(nullptr, MONGOAC_ERROR_CATEGORY_NONE, MONGOAC_ERROR_CODE_OK));
    }

    SECTION("default") {
        CHECK_NOTHROW(mongoac_error_set(error, MONGOAC_ERROR_CATEGORY_NONE, MONGOAC_ERROR_CODE_OK));
        CHECK(mongoac_error_category(error) == MONGOAC_ERROR_CATEGORY_NONE);
        CHECK(mongoac_error_code(error) == MONGOAC_ERROR_CODE_OK);
    }

    SECTION("custom") {
        auto const category = GENERATE(
            values<std::int32_t>({
                INT32_MIN,
                INT32_MIN + 1,
                -1,
                0,
                1,
                INT32_MAX - 1,
                INT32_MAX,
            }));

        auto const code = GENERATE(
            values<std::int32_t>({
                INT32_MIN,
                INT32_MIN + 1,
                -1,
                0,
                1,
                INT32_MAX - 1,
                INT32_MAX,
            }));

        CHECK_NOTHROW(mongoac_error_set(error, category, code));
        CHECK(mongoac_error_category(error) == category);
        CHECK(mongoac_error_code(error) == code);
    }
}
