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

#include <mongoac/find_one_options.h>

//

#include <catch2/catch_test_macros.hpp>

TEST_CASE("destroy", "[mongoac][find_one_options]") {
    SECTION("null") {
        CHECK_NOTHROW(mongoac_find_one_options_destroy(nullptr));
    }
}

TEST_CASE("clone", "[mongoac][find_one_options]") {
    SECTION("null") {
        CHECK(mongoac_find_one_options_clone(nullptr) == nullptr);
    }
}
