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

#include <mongoac/test/memory.hh>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstdint>

using mongoac::test::make_unique;

namespace {

constexpr mongoac_read_preference_mode_t mode_cases[] = {
    MONGOAC_READ_PREFERENCE_MODE_PRIMARY,
    MONGOAC_READ_PREFERENCE_MODE_SECONDARY,
    MONGOAC_READ_PREFERENCE_MODE_PRIMARY_PREFERRED,
    MONGOAC_READ_PREFERENCE_MODE_SECONDARY_PREFERRED,
    MONGOAC_READ_PREFERENCE_MODE_NEAREST,
};

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
