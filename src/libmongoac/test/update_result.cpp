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

#include <mongoac/update_result.h>

//

#include <mongoac/test/bson.hh>
#include <mongoac/test/error.hh>
#include <mongoac/test/memory.hh>
#include <mongoac/test/string.hh>

#include <mongoac/error.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using mongoac::test::from_mongoac;

TEST_CASE("destroy", "[mongoac][update_result]") {
    SECTION("null") {
        mongoac_update_result_destroy(nullptr);
        SUCCEED();
    }
}

TEST_CASE("clone", "[mongoac][update_result]") {
    SECTION("null") {
        CHECK(mongoac_update_result_clone(nullptr) == nullptr);
    }
}

TEST_CASE("get_matched_count", "[mongoac][update_result]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();

    SECTION("null") {
        CHECK(mongoac_update_result_get_matched_count(nullptr, error) == std::uint64_t{});
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
    }
}

TEST_CASE("get_modified_count", "[mongoac][update_result]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();

    SECTION("null") {
        CHECK(mongoac_update_result_get_modified_count(nullptr, error) == std::uint64_t{});
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
    }
}

TEST_CASE("get_upserted_id", "[mongoac][update_result]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();

    SECTION("null") {
        auto const bson = from_mongoac(mongoac_update_result_get_upserted_id(nullptr, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK(bson == nullptr);
    }
}
