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

#include <mongoac/run_command_options.h>

//

#include <mongoac/test/memory.hh>

#include <mongoac/read_preference.h>

#include <catch2/catch_test_macros.hpp>

using mongoac::test::make_unique;

TEST_CASE("destroy", "[mongoac][run_command_options]") {
    SECTION("null") {
        mongoac_run_command_options_destroy(nullptr);
        SUCCEED();
    }
}

TEST_CASE("clone", "[mongoac][run_command_options]") {
    SECTION("null") {
        auto const read_preference =
            make_unique(mongoac_read_preference_clone(nullptr), &mongoac_read_preference_destroy);

        CHECK(read_preference == nullptr);
    }

    SECTION("basic") {
        auto const source_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_command_options_new(), &mongoac_run_command_options_destroy);
        auto const source = source_owner.get();

        mongoac_run_command_options_set_read_preference(
            source, REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy).get());

        auto const target_owner =
            make_unique(mongoac_run_command_options_clone(source), &mongoac_run_command_options_destroy);
        auto const target = target_owner.get();

        CHECK(target != nullptr);
        CHECK(target != source);
        CHECK(mongoac_run_command_options_get_read_preference(target) != nullptr);
    }
}

TEST_CASE("new", "[mongoac][run_command_options]") {
    SECTION("default") {
        auto const options_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_run_command_options_new(), &mongoac_run_command_options_destroy);
        auto const options = options_owner.get();

        CHECK(mongoac_run_command_options_get_read_preference(options) == nullptr);
    }
}

TEST_CASE("get_read_preference", "[mongoac][run_command_options]") {
    auto const options_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_run_command_options_new(), &mongoac_run_command_options_destroy);
    auto const options = options_owner.get();

    SECTION("null") {
        CHECK(mongoac_run_command_options_get_read_preference(nullptr) == nullptr);
    }

    SECTION("default") {
        CHECK(mongoac_run_command_options_get_read_preference(options) == nullptr);
    }
}

TEST_CASE("set_read_preference", "[mongoac][run_command_options]") {
    auto const options_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_run_command_options_new(), &mongoac_run_command_options_destroy);
    auto const options = options_owner.get();

    auto const primary_owner = REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
    auto const primary = primary_owner.get();
    auto const secondary_owner = REQUIRE_MAKE_UNIQUE(mongoac_read_preference_new(), &mongoac_read_preference_destroy);
    auto const secondary = secondary_owner.get();

    mongoac_read_preference_set_mode(secondary, MONGOAC_READ_PREFERENCE_MODE_SECONDARY);
    mongoac_read_preference_set_max_staleness_from_secs(secondary, 123);

    SECTION("basic") {
        mongoac_run_command_options_set_read_preference(options, primary);

        auto const current = mongoac_run_command_options_get_read_preference(options);

        REQUIRE(current != nullptr);
        REQUIRE(current != primary);

        CHECK(mongoac_read_preference_get_max_staleness_is_none(current));
    }

    SECTION("null options") {
        mongoac_run_command_options_set_read_preference(nullptr, primary);
        SUCCEED();
    }

    SECTION("null value") {
        mongoac_run_command_options_set_read_preference(options, primary);
        CHECK(mongoac_run_command_options_get_read_preference(options) != nullptr);

        mongoac_run_command_options_set_read_preference(options, nullptr);
        CHECK(mongoac_run_command_options_get_read_preference(options) == nullptr);
    }

    SECTION("overwrite") {
        mongoac_run_command_options_set_read_preference(options, primary);

        {
            auto const v = mongoac_run_command_options_get_read_preference(options);

            REQUIRE(v != nullptr);
            REQUIRE(v != primary);
            CHECK(mongoac_read_preference_get_max_staleness_is_none(v));
        }

        mongoac_run_command_options_set_read_preference(options, secondary);

        {
            auto const v = mongoac_run_command_options_get_read_preference(options);

            REQUIRE(v != nullptr);
            REQUIRE(v != secondary);
            CHECK_FALSE(mongoac_read_preference_get_max_staleness_is_none(v));
            CHECK(mongoac_read_preference_get_max_staleness_as_secs(v) == 123);
        }
    }
}
