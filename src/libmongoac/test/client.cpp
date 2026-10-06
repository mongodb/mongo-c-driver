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

#include <mongoac/client.h>

//

#include <mongoac/test/memory.hh>
#include <mongoac/test/string.hh>

#include <mongoac/error.h>
#include <mongoac/runtime.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using mongoac::test::operator""_sv;
using mongoac::test::from_mongoac;
using mongoac::test::make_unique;

namespace {

constexpr auto default_uri = "mongodb://doesnotexist.invalid?serverSelectionTimeoutMS=1000"_sv;

} // namespace

TEST_CASE("destroy", "[mongoac][client]") {
    SECTION("null") {
        CHECK_NOTHROW(mongoac_client_destroy(nullptr));
    }
}

TEST_CASE("new", "[mongoac][client]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();

    SECTION("null connection string") {
        auto const client = make_unique(mongoac_client_new({}, error), &mongoac_client_destroy);

        CHECK(client == nullptr);
        CHECK(mongoac_error_category(error) == MONGOAC_ERROR_CATEGORY_MONGOAC);
        CHECK(mongoac_error_code(error) == MONGOAC_ERROR_CODE_INVALID_ARGUMENT);
    }

    SECTION("invalid UTF-8") {
        auto const client = make_unique(mongoac_client_new("\xff"_sv, error), &mongoac_client_destroy);

        CHECK(client == nullptr);
        CHECK(mongoac_error_category(error) == MONGOAC_ERROR_CATEGORY_MONGOAC);
        CHECK(mongoac_error_code(error) == MONGOAC_ERROR_CODE_INVALID_ARGUMENT);
        CHECK_THAT(from_mongoac(mongoac_error_message(error)), Catch::Matchers::ContainsSubstring("invalid UTF-8"));
    }

    SECTION("invalid connection string") {
        auto const client = make_unique(mongoac_client_new(""_sv, error), &mongoac_client_destroy);

        CHECK(client == nullptr);
        CHECK(mongoac_error_category(error) == MONGOAC_ERROR_CATEGORY_RUST);
        CHECK_THAT(
            from_mongoac(mongoac_error_message(error)), Catch::Matchers::ContainsSubstring("contains no scheme"));
    }

    SECTION("valid") {
        auto const client = make_unique(mongoac_client_new(default_uri, error), &mongoac_client_destroy);

        CHECK(client != nullptr);
        CHECK(mongoac_error_category(error) == MONGOAC_ERROR_CATEGORY_NONE);
        CHECK(mongoac_error_code(error) == MONGOAC_ERROR_CODE_OK);
    }
}

TEST_CASE("clone", "[mongoac][client]") {
    auto const client_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri, nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();

    SECTION("null") {
        auto const copy = make_unique(mongoac_client_clone(nullptr), &mongoac_client_destroy);
        CHECK(copy == nullptr);
    }

    SECTION("default") {
        auto const copy_owner = make_unique(mongoac_client_clone(client), &mongoac_client_destroy);
        auto const copy = copy_owner.get();
        CHECK(copy != nullptr);

        CHECK(client != copy);

        auto const rt_orig = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
        auto const rt_copy = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(copy), &mongoac_runtime_destroy);

        CHECK(mongoac_runtime_address(rt_orig.get()) == mongoac_runtime_address(rt_copy.get()));

        SUCCEED("TODO: validate URI options are inherited using command monitoring events");
    }

    SECTION("custom") {
        SUCCEED("TODO: validate URI options are inherited using command monitoring events");
    }
}

TEST_CASE("get_runtime", "[mongoac][client]") {
    SECTION("null") {
        auto const runtime = make_unique(mongoac_client_get_runtime(nullptr), &mongoac_runtime_destroy);
        CHECK(runtime.get() == nullptr);
    }

    SECTION("default") {
        auto const client = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri, nullptr), &mongoac_client_destroy);

        auto const runtime = make_unique(mongoac_client_get_runtime(client.get()), &mongoac_runtime_destroy);
        CHECK(runtime.get() != nullptr);
    }

    SECTION("unique") {
        auto const c1 = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri, nullptr), &mongoac_client_destroy);
        auto const r1 = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(c1.get()), &mongoac_runtime_destroy);
        auto const c2 = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri, nullptr), &mongoac_client_destroy);
        auto const r2 = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(c2.get()), &mongoac_runtime_destroy);

        CHECK(mongoac_runtime_address(r1.get()) != mongoac_runtime_address(r2.get()));
    }

    SECTION("shared") {
        auto const c1 = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri, nullptr), &mongoac_client_destroy);
        auto const r1 = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(c1.get()), &mongoac_runtime_destroy);
        auto const c2 = REQUIRE_MAKE_UNIQUE(mongoac_client_clone(c1.get()), &mongoac_client_destroy);
        auto const r2 = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(c2.get()), &mongoac_runtime_destroy);

        CHECK(mongoac_runtime_address(r1.get()) == mongoac_runtime_address(r2.get()));
    }
}

TEST_CASE("shutdown", "[mongoac][client]") {
    SECTION("null") {
        CHECK_NOTHROW(mongoac_client_shutdown(nullptr));
    }

    SECTION("default") {
        auto const client = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri, nullptr), &mongoac_client_destroy);
        CHECK_NOTHROW(mongoac_client_shutdown(client.get()));
    }
}
