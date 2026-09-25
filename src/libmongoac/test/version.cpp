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

#include <mongoac/version.h>

//

#include <mongoac/test/string.hh>

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using mongoac::test::from_mongoac;

TEST_CASE("constants", "[mongoac][version]") {
    SECTION("major") {
        CHECK(MONGOAC_VERSION_MAJOR == mongoac_version_major());
    }

    SECTION("minor") {
        CHECK(MONGOAC_VERSION_MINOR == mongoac_version_minor());
    }

    SECTION("patch") {
        CHECK(MONGOAC_VERSION_PATCH == mongoac_version_patch());
    }

    SECTION("prerelease") {
        CHECK(MONGOAC_VERSION_PRERELEASE == from_mongoac(mongoac_version_prerelease()));
    }

    SECTION("full") {
        CHECK(MONGOAC_VERSION == from_mongoac(mongoac_version()));
    }

    SECTION("hex") {
        CHECK(MONGOAC_VERSION_HEX == mongoac_version_hex());
    }
}

TEST_CASE("check", "[mongoac][version]") {
    SECTION("macro") {
        static constexpr auto const major = MONGOAC_VERSION_MAJOR;
        static constexpr auto const minor = MONGOAC_VERSION_MINOR;
        static constexpr auto const patch = MONGOAC_VERSION_PATCH;

        CHECK(MONGOAC_CHECK_VERSION(major, minor, patch));

        CHECK(MONGOAC_CHECK_VERSION(major, minor, patch - 1));
        CHECK(MONGOAC_CHECK_VERSION(major, minor - 1, patch));
        CHECK(MONGOAC_CHECK_VERSION(major - 1, minor, patch));

        CHECK_FALSE(MONGOAC_CHECK_VERSION(major, minor, patch + 1));
        CHECK_FALSE(MONGOAC_CHECK_VERSION(major, minor + 1, patch));
        CHECK_FALSE(MONGOAC_CHECK_VERSION(major + 1, minor, patch));

        CHECK_FALSE(MONGOAC_CHECK_VERSION(major, minor + 1, 0));
        CHECK_FALSE(MONGOAC_CHECK_VERSION(major + 1, 0, 0));
    }

    SECTION("function") {
        auto const major = mongoac_version_major();
        auto const minor = mongoac_version_minor();
        auto const patch = mongoac_version_patch();

        CHECK(mongoac_check_version(major, minor, patch));

        CHECK(mongoac_check_version(major, minor, patch - 1));
        CHECK(mongoac_check_version(major, minor - 1, patch));
        CHECK(mongoac_check_version(major - 1, minor, patch));

        CHECK_FALSE(mongoac_check_version(major, minor, patch + 1));
        CHECK_FALSE(mongoac_check_version(major, minor + 1, patch));
        CHECK_FALSE(mongoac_check_version(major + 1, minor, patch));

        CHECK_FALSE(mongoac_check_version(major, minor + 1, 0));
        CHECK_FALSE(mongoac_check_version(major + 1, 0, 0));
    }
}
