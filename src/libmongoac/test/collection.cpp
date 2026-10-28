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

#include <mongoac/collection.h>

//

#include <mongoac/test/bson.hh>
#include <mongoac/test/client.hh>
#include <mongoac/test/error.hh>
#include <mongoac/test/memory.hh>
#include <mongoac/test/string.hh>

#include <mongoac/client.h>
#include <mongoac/database.h>
#include <mongoac/error.h>
#include <mongoac/future.h>
#include <mongoac/runtime.h>

#include <bson/bson.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>
#include <string_view>

using mongoac::test::doesnotexist_uri;
using mongoac::test::from_mongoac;
using mongoac::test::make_unique;
using mongoac::test::operator""_sv;
using mongoac::test::operator""_bson;
using mongoac::test::default_uri;
using mongoac::test::to_mongoac;

TEST_CASE("destroy", "[mongoac][collection]") {
    SECTION("null") {
        CHECK_NOTHROW(mongoac_collection_destroy(nullptr));
    }
}

TEST_CASE("clone", "[mongoac][collection]") {
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const db_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_get_database(client, to_mongoac("db"), nullptr), &mongoac_database_destroy);
    auto const db = db_owner.get();
    auto const coll_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_database_get_collection(db, "coll"_sv, nullptr), &mongoac_collection_destroy);
    auto const coll = coll_owner.get();

    SECTION("null") {
        auto const copy = make_unique(mongoac_collection_clone(nullptr), &mongoac_collection_destroy);
        CHECK(copy == nullptr);
    }

    SECTION("basic") {
        auto const copy_owner = make_unique(mongoac_collection_clone(coll), &mongoac_collection_destroy);
        auto const copy = copy_owner.get();
        CHECK(copy != nullptr);

        CHECK(coll != copy);

        auto const rt_orig = REQUIRE_MAKE_UNIQUE(mongoac_collection_get_runtime(coll), &mongoac_runtime_destroy);
        auto const rt_copy = REQUIRE_MAKE_UNIQUE(mongoac_collection_get_runtime(copy), &mongoac_runtime_destroy);

        CHECK(mongoac_runtime_address(rt_orig.get()) == mongoac_runtime_address(rt_copy.get()));
        CHECK(from_mongoac(mongoac_collection_get_name(coll)) == from_mongoac(mongoac_collection_get_name(copy)));

        SUCCEED("TODO: validate collection options are inherited using command monitoring events");
    }

    SECTION("custom") {
        SUCCEED("TODO: validate collection options are inherited using command monitoring events");
    }
}

TEST_CASE("get_client", "[mongoac][collection]") {
    SECTION("null") {
        CHECK(mongoac_collection_get_client(nullptr) == nullptr);
    }

    SECTION("basic") {
        auto const c1 = REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const db =
            REQUIRE_MAKE_UNIQUE(mongoac_client_get_database(c1.get(), "db"_sv, nullptr), &mongoac_database_destroy);
        auto const coll = REQUIRE_MAKE_UNIQUE(
            mongoac_database_get_collection(db.get(), "coll"_sv, nullptr), &mongoac_collection_destroy);

        auto const c2 = REQUIRE_MAKE_UNIQUE(mongoac_collection_get_client(coll.get()), &mongoac_client_destroy);
        CHECK(c2.get() != nullptr);
        CHECK(c1.get() != c2.get());
        CHECK(mongoac_client_get_runtime_address(c1.get()) == mongoac_client_get_runtime_address(c2.get()));
    }
}

TEST_CASE("get_name", "[mongoac][collection]") {
    SECTION("null") {
        CHECK(from_mongoac(mongoac_collection_get_name(nullptr)) == "");
    }

    SECTION("basic") {
        auto const client_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const client = client_owner.get();
        auto const db_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_client_get_database(client, to_mongoac("db"), nullptr), &mongoac_database_destroy);
        auto const db = db_owner.get();

        auto const name = GENERATE(values<std::string_view>({"", "coll", "a.b.c"}));

        auto const coll = REQUIRE_MAKE_UNIQUE(
            mongoac_database_get_collection(db, to_mongoac(name), nullptr), &mongoac_collection_destroy);

        CHECK(from_mongoac(mongoac_collection_get_name(coll.get())) == name);
    }
}

TEST_CASE("count_documents", "[mongoac][collection]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();

    auto const client_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const db_owner = REQUIRE_MAKE_UNIQUE(
        mongoac_client_get_database(client, "mongoac_collection_count_documents"_sv, nullptr),
        &mongoac_database_destroy);
    auto const db = db_owner.get();
    auto const coll_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_database_get_collection(db, "coll"_sv, nullptr), &mongoac_collection_destroy);
    auto const coll = coll_owner.get();

    // Start from a known state. A non-existing collection counts as zero documents.
    CHECK_NOTHROW(mongoac_database_drop(db, nullptr, nullptr, error));
    REQUIRE_MONGOAC_ERROR_OK(error);

    SECTION("null coll") {
        CHECK(mongoac_collection_count_documents(nullptr, nullptr, {}, nullptr, error) == 0u);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
    }

    SECTION("null filter") {
        CHECK(mongoac_collection_count_documents(coll, nullptr, {}, nullptr, error) == 0u);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
    }

    SECTION("invalid BSON filter") {
        std::uint8_t const bytes[] = {0xff};

        CHECK(mongoac_collection_count_documents(coll, nullptr, {bytes, sizeof(bytes)}, nullptr, error) == 0u);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
    }

    SECTION("basic") {
        auto const filter_all = REQUIRE_MAKE_UNIQUE("{}"_bson, &bson_destroy);
        CHECK(mongoac_collection_count_documents(coll, nullptr, to_mongoac(filter_all.get()), nullptr, error) == 0u);
        CHECK_MONGOAC_ERROR_OK(error);

        auto const insert = REQUIRE_MAKE_UNIQUE(
            R"({"insert": "coll", "documents": [{"x": 1}, {"x": 2}, {"x": 3}]})"_bson, &bson_destroy);

        (void)from_mongoac(mongoac_database_run_command(db, nullptr, to_mongoac(insert.get()), nullptr, error));
        REQUIRE_MONGOAC_ERROR_OK(error);

        CHECK(mongoac_collection_count_documents(coll, nullptr, to_mongoac(filter_all.get()), nullptr, error) == 3u);
        CHECK_MONGOAC_ERROR_OK(error);

        auto const filter_x2 = REQUIRE_MAKE_UNIQUE(R"({"x": 2})"_bson, &bson_destroy);
        CHECK(mongoac_collection_count_documents(coll, nullptr, to_mongoac(filter_x2.get()), nullptr, error) == 1u);
        CHECK_MONGOAC_ERROR_OK(error);
    }

    SECTION("rejected filter operator") {
        // Per the CRUD spec, countDocuments uses a $match stage, so $where is not supported.
        // The command is forwarded to the server, which rejects it.
        auto const filter_where = REQUIRE_MAKE_UNIQUE(R"({"$where": "true"})"_bson, &bson_destroy);

        CHECK(mongoac_collection_count_documents(coll, nullptr, to_mongoac(filter_where.get()), nullptr, error) == 0u);
        CHECK_MONGOAC_ERROR_CATEGORY(error, MONGOAC_ERROR_CATEGORY_SERVER);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "$where is not allowed");
    }
}

TEST_CASE("count_documents_async", "[mongoac][collection]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();

    auto const client_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const db_owner = REQUIRE_MAKE_UNIQUE(
        mongoac_client_get_database(client, "mongoac_collection_count_documents_async"_sv, nullptr),
        &mongoac_database_destroy);
    auto const db = db_owner.get();
    auto const coll_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_database_get_collection(db, "coll"_sv, nullptr), &mongoac_collection_destroy);
    auto const coll = coll_owner.get();

    // Start from a known state. A non-existing collection counts as zero documents.
    CHECK_NOTHROW(mongoac_database_drop(db, nullptr, nullptr, error));
    REQUIRE_MONGOAC_ERROR_OK(error);

    SECTION("null coll") {
        auto const future = make_unique(
            mongoac_collection_count_documents_async(nullptr, nullptr, {}, nullptr), &mongoac_future_destroy);
        CHECK(future.get() == nullptr);
    }

    SECTION("null filter") {
        auto const future =
            make_unique(mongoac_collection_count_documents_async(coll, nullptr, {}, nullptr), &mongoac_future_destroy);
        CHECK(future.get() == nullptr);
    }

    SECTION("invalid BSON filter") {
        std::uint8_t const bytes[] = {0xff};

        auto const future = make_unique(
            mongoac_collection_count_documents_async(coll, nullptr, {bytes, sizeof(bytes)}, nullptr),
            &mongoac_future_destroy);
        CHECK(future.get() == nullptr);
    }

    SECTION("rejected filter operator") {
        // Per the CRUD spec, countDocuments uses a $match stage, so $where is not supported.
        // The command is forwarded to the server, which rejects it; the error surfaces via the future.
        auto const filter_where = REQUIRE_MAKE_UNIQUE(R"({"$where": "true"})"_bson, &bson_destroy);

        auto const future_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_collection_count_documents_async(coll, nullptr, to_mongoac(filter_where.get()), nullptr),
            &mongoac_future_destroy);
        auto const future = future_owner.get();

        {
            auto const runtime = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
            CHECK_NOTHROW(mongoac_runtime_block_on(runtime.get(), future, error));
            CHECK_MONGOAC_ERROR_OK(error);
        }

        CHECK(mongoac_future_get_uint64(future, error) == 0u);
        CHECK_MONGOAC_ERROR_CATEGORY(error, MONGOAC_ERROR_CATEGORY_SERVER);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "$where is not allowed");
    }

    SECTION("basic") {
        auto const filter_all = REQUIRE_MAKE_UNIQUE("{}"_bson, &bson_destroy);

        {
            auto const future_owner = REQUIRE_MAKE_UNIQUE(
                mongoac_collection_count_documents_async(coll, nullptr, to_mongoac(filter_all.get()), nullptr),
                &mongoac_future_destroy);
            auto const future = future_owner.get();

            {
                auto const runtime = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
                CHECK_NOTHROW(mongoac_runtime_block_on(runtime.get(), future, error));
                CHECK_MONGOAC_ERROR_OK(error);
            }

            CHECK(mongoac_future_get_uint64(future, error) == 0u);
            CHECK_MONGOAC_ERROR_OK(error);
        }

        auto const insert = REQUIRE_MAKE_UNIQUE(
            R"({"insert": "coll", "documents": [{"x": 1}, {"x": 2}, {"x": 3}]})"_bson, &bson_destroy);

        (void)from_mongoac(mongoac_database_run_command(db, nullptr, to_mongoac(insert.get()), nullptr, error));
        REQUIRE_MONGOAC_ERROR_OK(error);

        auto const filter_x2 = REQUIRE_MAKE_UNIQUE(R"({"x": 2})"_bson, &bson_destroy);

        auto const future_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_collection_count_documents_async(coll, nullptr, to_mongoac(filter_x2.get()), nullptr),
            &mongoac_future_destroy);
        auto const future = future_owner.get();

        {
            auto const runtime = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
            CHECK_NOTHROW(mongoac_runtime_block_on(runtime.get(), future, error));
            CHECK_MONGOAC_ERROR_OK(error);
        }

        CHECK(mongoac_future_get_uint64(future, error) == 1u);
        CHECK_MONGOAC_ERROR_OK(error);
    }
}

TEST_CASE("drop", "[mongoac][collection]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();

    SECTION("null") {
        CHECK_NOTHROW(mongoac_collection_drop(nullptr, nullptr, nullptr, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
    }

    SECTION("basic") {
        auto const client_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
        auto const client = client_owner.get();
        auto const db_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_get_database(client, "db"_sv, nullptr), &mongoac_database_destroy);
        auto const db = db_owner.get();
        auto const coll_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_database_get_collection(db, "coll"_sv, nullptr), &mongoac_collection_destroy);
        auto const coll = coll_owner.get();

        CHECK_NOTHROW(mongoac_collection_drop(coll, nullptr, nullptr, error));
        CHECK_MONGOAC_ERROR_OK(error);
    }
}

TEST_CASE("drop_async", "[mongoac][collection]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();

    SECTION("null") {
        auto const future =
            make_unique(mongoac_collection_drop_async(nullptr, nullptr, nullptr), &mongoac_future_destroy);
        CHECK(future.get() == nullptr);
    }

    SECTION("basic") {
        auto const client_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
        auto const client = client_owner.get();
        auto const db_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_get_database(client, "db"_sv, nullptr), &mongoac_database_destroy);
        auto const db = db_owner.get();
        auto const coll_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_database_get_collection(db, "coll"_sv, nullptr), &mongoac_collection_destroy);
        auto const coll = coll_owner.get();

        auto const future_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_collection_drop_async(coll, nullptr, nullptr), &mongoac_future_destroy);
        auto const future = future_owner.get();

        {
            auto const runtime = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
            CHECK_NOTHROW(mongoac_runtime_block_on(runtime.get(), future, error));
            CHECK_MONGOAC_ERROR_OK(error);
        }

        CHECK_NOTHROW(mongoac_future_get_void(future, error));
        CHECK_MONGOAC_ERROR_OK(error);
    }
}

TEST_CASE("get_runtime", "[mongoac][collection]") {
    SECTION("null") {
        auto const runtime = make_unique(mongoac_collection_get_runtime(nullptr), &mongoac_runtime_destroy);
        CHECK(runtime.get() == nullptr);
    }

    SECTION("basic") {
        auto const client_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const client = client_owner.get();
        auto const db_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_client_get_database(client, to_mongoac("db"), nullptr), &mongoac_database_destroy);
        auto const db = db_owner.get();

        auto const coll =
            REQUIRE_MAKE_UNIQUE(mongoac_database_get_collection(db, "coll"_sv, nullptr), &mongoac_collection_destroy);

        auto const runtime = make_unique(mongoac_collection_get_runtime(coll.get()), &mongoac_runtime_destroy);
        CHECK(runtime.get() != nullptr);
    }

    SECTION("unique") {
        auto const c1 = REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const c2 = REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const d1 = REQUIRE_MAKE_UNIQUE(
            mongoac_client_get_database(c1.get(), to_mongoac("db"), nullptr), &mongoac_database_destroy);
        auto const d2 = REQUIRE_MAKE_UNIQUE(
            mongoac_client_get_database(c2.get(), to_mongoac("db"), nullptr), &mongoac_database_destroy);

        auto const coll1 = REQUIRE_MAKE_UNIQUE(
            mongoac_database_get_collection(d1.get(), "coll"_sv, nullptr), &mongoac_collection_destroy);
        auto const coll2 = REQUIRE_MAKE_UNIQUE(
            mongoac_database_get_collection(d2.get(), "coll"_sv, nullptr), &mongoac_collection_destroy);

        auto const r1 = REQUIRE_MAKE_UNIQUE(mongoac_collection_get_runtime(coll1.get()), &mongoac_runtime_destroy);
        auto const r2 = REQUIRE_MAKE_UNIQUE(mongoac_collection_get_runtime(coll2.get()), &mongoac_runtime_destroy);

        CHECK(mongoac_runtime_address(r1.get()) != mongoac_runtime_address(r2.get()));
    }

    SECTION("shared") {
        auto const client =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const db = REQUIRE_MAKE_UNIQUE(
            mongoac_client_get_database(client.get(), to_mongoac("db"), nullptr), &mongoac_database_destroy);

        auto const c1 = REQUIRE_MAKE_UNIQUE(
            mongoac_database_get_collection(db.get(), "coll"_sv, nullptr), &mongoac_collection_destroy);
        auto const c2 = REQUIRE_MAKE_UNIQUE(mongoac_collection_clone(c1.get()), &mongoac_collection_destroy);

        auto const r1 = REQUIRE_MAKE_UNIQUE(mongoac_collection_get_runtime(c1.get()), &mongoac_runtime_destroy);
        auto const r2 = REQUIRE_MAKE_UNIQUE(mongoac_collection_get_runtime(c2.get()), &mongoac_runtime_destroy);

        CHECK(mongoac_runtime_address(r1.get()) == mongoac_runtime_address(r2.get()));
    }
}

TEST_CASE("get_runtime_address", "[mongoac][collection]") {
    SECTION("null") {
        CHECK(mongoac_collection_get_runtime_address(nullptr) == std::uintptr_t{});
    }

    SECTION("basic") {
        auto const client =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const db =
            REQUIRE_MAKE_UNIQUE(mongoac_client_get_database(client.get(), "db"_sv, nullptr), &mongoac_database_destroy);
        auto const coll = REQUIRE_MAKE_UNIQUE(
            mongoac_database_get_collection(db.get(), "coll"_sv, nullptr), &mongoac_collection_destroy);

        auto const runtime = REQUIRE_MAKE_UNIQUE(mongoac_collection_get_runtime(coll.get()), &mongoac_runtime_destroy);
        CHECK(mongoac_runtime_address(runtime.get()) == mongoac_collection_get_runtime_address(coll.get()));
    }
}
