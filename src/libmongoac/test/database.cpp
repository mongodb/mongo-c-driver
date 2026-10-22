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

#include <mongoac/database.h>

//

#include <mongoac/test/client.hh>
#include <mongoac/test/memory.hh>
#include <mongoac/test/string.hh>

#include <mongoac/client.h>
#include <mongoac/collection.h>
#include <mongoac/runtime.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstdint>
#include <string_view>

using mongoac::test::doesnotexist_uri;
using mongoac::test::from_mongoac;
using mongoac::test::make_unique;
using mongoac::test::operator""_sv;
using mongoac::test::to_mongoac;

TEST_CASE("destroy", "[mongoac][database]") {
    SECTION("null") {
        CHECK_NOTHROW(mongoac_database_destroy(nullptr));
    }
}

TEST_CASE("clone", "[mongoac][database]") {
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const db_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_get_database(client, "db"_sv, nullptr), &mongoac_database_destroy);
    auto const db = db_owner.get();

    SECTION("null") {
        auto const copy = make_unique(mongoac_database_clone(nullptr), &mongoac_database_destroy);
        CHECK(copy == nullptr);
    }

    SECTION("basic") {
        auto const copy_owner = make_unique(mongoac_database_clone(db), &mongoac_database_destroy);
        auto const copy = copy_owner.get();
        CHECK(copy != nullptr);

        CHECK(db != copy);

        auto const rt_orig = REQUIRE_MAKE_UNIQUE(mongoac_database_get_runtime(db), &mongoac_runtime_destroy);
        auto const rt_copy = REQUIRE_MAKE_UNIQUE(mongoac_database_get_runtime(copy), &mongoac_runtime_destroy);

        CHECK(mongoac_runtime_address(rt_orig.get()) == mongoac_runtime_address(rt_copy.get()));
        CHECK(from_mongoac(mongoac_database_get_name(db)) == from_mongoac(mongoac_database_get_name(copy)));

        SUCCEED("TODO: validate database options are inherited using command monitoring events");
    }

    SECTION("custom") {
        SUCCEED("TODO: validate database options are inherited using command monitoring events");
    }
}

TEST_CASE("get_client", "[mongoac][database]") {
    SECTION("null") {
        CHECK(mongoac_database_get_client(nullptr) == nullptr);
    }

    SECTION("basic") {
        auto const c1 = REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const db =
            REQUIRE_MAKE_UNIQUE(mongoac_client_get_database(c1.get(), "db"_sv, nullptr), &mongoac_database_destroy);

        auto const c2 = REQUIRE_MAKE_UNIQUE(mongoac_database_get_client(db.get()), &mongoac_client_destroy);
        CHECK(c2.get() != nullptr);
        CHECK(c1.get() != c2.get());
        CHECK(mongoac_client_get_runtime_address(c1.get()) == mongoac_client_get_runtime_address(c2.get()));
    }
}

TEST_CASE("get_collection", "[mongoac][database]") {
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const db_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_get_database(client, "db"_sv, nullptr), &mongoac_database_destroy);
    auto const db = db_owner.get();

    SECTION("null client") {
        auto const coll =
            make_unique(mongoac_database_get_collection(nullptr, {}, nullptr), &mongoac_collection_destroy);
        CHECK(coll.get() == nullptr);
    }

    SECTION("null name") {
        auto const coll = make_unique(mongoac_database_get_collection(db, {}, nullptr), &mongoac_collection_destroy);
        CHECK(coll.get() == nullptr);
    }

    SECTION("basic") {
        auto const name = GENERATE(values<std::string_view>({"", "coll", "a.b.c"}));
        auto const coll =
            make_unique(mongoac_database_get_collection(db, to_mongoac(name), nullptr), &mongoac_collection_destroy);
        CHECK(coll.get() != nullptr);
        CHECK(from_mongoac(mongoac_collection_get_name(coll.get())) == name);
    }
}

TEST_CASE("get_name", "[mongoac][database]") {
    SECTION("null") {
        CHECK(from_mongoac(mongoac_database_get_name(nullptr)) == "");
    }

    SECTION("basic") {
        auto const client =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);

        auto const name = GENERATE(values<std::string_view>({"", "db", "a.b.c"}));

        auto const db = REQUIRE_MAKE_UNIQUE(
            mongoac_client_get_database(client.get(), to_mongoac(name), nullptr), &mongoac_database_destroy);

        CHECK(from_mongoac(mongoac_database_get_name(db.get())) == name);
    }
}

TEST_CASE("get_runtime", "[mongoac][database]") {
    SECTION("null") {
        auto const runtime = make_unique(mongoac_database_get_runtime(nullptr), &mongoac_runtime_destroy);
        CHECK(runtime.get() == nullptr);
    }

    SECTION("basic") {
        auto const client_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const client = client_owner.get();

        auto const db =
            REQUIRE_MAKE_UNIQUE(mongoac_client_get_database(client, "db"_sv, nullptr), &mongoac_database_destroy);

        auto const runtime = make_unique(mongoac_database_get_runtime(db.get()), &mongoac_runtime_destroy);
        CHECK(runtime.get() != nullptr);
    }

    SECTION("unique") {
        auto const c1 = REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const c2 = REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);

        auto const d1 =
            REQUIRE_MAKE_UNIQUE(mongoac_client_get_database(c1.get(), "db"_sv, nullptr), &mongoac_database_destroy);
        auto const d2 =
            REQUIRE_MAKE_UNIQUE(mongoac_client_get_database(c2.get(), "db"_sv, nullptr), &mongoac_database_destroy);

        auto const r1 = REQUIRE_MAKE_UNIQUE(mongoac_database_get_runtime(d1.get()), &mongoac_runtime_destroy);
        auto const r2 = REQUIRE_MAKE_UNIQUE(mongoac_database_get_runtime(d2.get()), &mongoac_runtime_destroy);

        CHECK(mongoac_runtime_address(r1.get()) != mongoac_runtime_address(r2.get()));
    }

    SECTION("shared") {
        auto const client =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);

        auto const d1 =
            REQUIRE_MAKE_UNIQUE(mongoac_client_get_database(client.get(), "db"_sv, nullptr), &mongoac_database_destroy);
        auto const d2 = REQUIRE_MAKE_UNIQUE(mongoac_database_clone(d1.get()), &mongoac_database_destroy);

        auto const r1 = REQUIRE_MAKE_UNIQUE(mongoac_database_get_runtime(d1.get()), &mongoac_runtime_destroy);
        auto const r2 = REQUIRE_MAKE_UNIQUE(mongoac_database_get_runtime(d2.get()), &mongoac_runtime_destroy);

        CHECK(mongoac_runtime_address(r1.get()) == mongoac_runtime_address(r2.get()));
    }
}

TEST_CASE("get_runtime_address", "[mongoac][database]") {
    SECTION("null") {
        CHECK(mongoac_database_get_runtime_address(nullptr) == std::uintptr_t{});
    }

    SECTION("basic") {
        auto const client =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const db =
            REQUIRE_MAKE_UNIQUE(mongoac_client_get_database(client.get(), "db"_sv, nullptr), &mongoac_database_destroy);

        auto const runtime = REQUIRE_MAKE_UNIQUE(mongoac_database_get_runtime(db.get()), &mongoac_runtime_destroy);
        CHECK(mongoac_runtime_address(runtime.get()) == mongoac_database_get_runtime_address(db.get()));
    }
}
