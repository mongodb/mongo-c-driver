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

#include <mongoac/client_session.h>

//

#include <mongoac/test/bson.hh>
#include <mongoac/test/error.hh>
#include <mongoac/test/memory.hh>
#include <mongoac/test/string.hh>

#include <mongoac/client_session.h>
#include <mongoac/error.h>

#include <catch2/catch_test_macros.hpp>

using mongoac::test::from_mongoac;
using mongoac::test::make_unique;

TEST_CASE("destroy", "[mongoac][client_session]") {
    SECTION("null") {
        mongoac_client_session_destroy(nullptr);
        SUCCEED();
    }
}

TEST_CASE("clone", "[mongoac][client_session]") {
    SECTION("null") {
        CHECK(mongoac_client_session_clone(nullptr) == nullptr);
    }
}

TEST_CASE("get_id", "[mongoac][client_session]") {
    SECTION("null") {
        auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
        auto const error = error_owner.get();

        auto const bson = from_mongoac(mongoac_client_session_get_id(nullptr, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK(bson == nullptr);
    }
}
