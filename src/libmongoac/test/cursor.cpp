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

#include <mongoac/cursor.h>

//

#include <mongoac/test/bson.hh>
#include <mongoac/test/client.hh>
#include <mongoac/test/error.hh>
#include <mongoac/test/memory.hh>
#include <mongoac/test/string.hh>

#include <mongoac/client.h>
#include <mongoac/collection.h>
#include <mongoac/database.h>
#include <mongoac/error.h>
#include <mongoac/future.h>
#include <mongoac/insert_one_result.h>
#include <mongoac/runtime.h>

#include <bson/bson.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <set>

using mongoac::test::default_uri;
using mongoac::test::from_mongoac;
using mongoac::test::make_unique;
using mongoac::test::operator""_bson;
using mongoac::test::operator""_sv;
using mongoac::test::to_bson;
using mongoac::test::to_mongoac;

TEST_CASE("destroy", "[mongoac][cursor]") {
    SECTION("null") {
        mongoac_cursor_destroy(nullptr);
        SUCCEED();
    }
}

TEST_CASE("clone", "[mongoac][cursor]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const db_owner = REQUIRE_MAKE_UNIQUE(
        mongoac_client_get_database(client, "mongoac_cursor_clone"_sv, nullptr), &mongoac_database_destroy);
    auto const db = db_owner.get();

    mongoac_database_drop(db, nullptr, nullptr, error);
    REQUIRE_MONGOAC_ERROR_OK(error);

    SECTION("null") {
        auto const copy = make_unique(mongoac_cursor_clone(nullptr), &mongoac_cursor_destroy);
        CHECK(copy == nullptr);
    }

    SECTION("basic") {
        auto const coll_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_database_get_collection(db, "coll"_sv, nullptr), &mongoac_collection_destroy);
        auto const coll = coll_owner.get();

        (void)make_unique(
            mongoac_collection_insert_one(
                coll,
                nullptr,
                to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"_id": 1})"_bson, &bson_destroy).get()),
                nullptr,
                error),
            &mongoac_insert_one_result_destroy);
        (void)make_unique(
            mongoac_collection_insert_one(
                coll,
                nullptr,
                to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"_id": 2})"_bson, &bson_destroy).get()),
                nullptr,
                error),
            &mongoac_insert_one_result_destroy);
        REQUIRE_MONGOAC_ERROR_OK(error);

        auto const cursor_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_database_run_cursor_command(
                db,
                nullptr,
                to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"find": "coll", "sort": {"_id": 1}})"_bson, &bson_destroy).get()),
                nullptr,
                error),
            &mongoac_cursor_destroy);
        REQUIRE_MONGOAC_ERROR_OK(error);
        auto const cursor = cursor_owner.get();

        // Shared state.
        {
            auto const copy_owner = REQUIRE_MAKE_UNIQUE(mongoac_cursor_clone(cursor), &mongoac_cursor_destroy);
            auto const copy = copy_owner.get();

            auto const doc1_owner = from_mongoac(mongoac_cursor_next(copy, error));
            CHECK_MONGOAC_ERROR_OK(error);
            auto const doc2_owner = from_mongoac(mongoac_cursor_next(copy, error));
            CHECK_MONGOAC_ERROR_OK(error);

            auto const doc1 = REQUIRE_BSON_STATIC(doc1_owner);
            auto const doc2 = REQUIRE_BSON_STATIC(doc2_owner);
            auto const id1 = REQUIRE_BSON_ITER_FIND(doc1, "_id");
            auto const id2 = REQUIRE_BSON_ITER_FIND(doc2, "_id");

            CHECK(bson_iter_int32(&id1) == 1);
            CHECK(bson_iter_int32(&id2) == 2);
        }

        // Exhausted by `copy`.
        {
            auto const done = from_mongoac(mongoac_cursor_next(cursor, error));
            CHECK_MONGOAC_ERROR_OK(error);
            auto const idempotency = from_mongoac(mongoac_cursor_next(cursor, error));
            CHECK_MONGOAC_ERROR_OK(error);

            CHECK(done == nullptr);
            CHECK(idempotency == nullptr);
        }
    }
}

TEST_CASE("next", "[mongoac][cursor]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const db_owner = REQUIRE_MAKE_UNIQUE(
        mongoac_client_get_database(client, "mongoac_cursor_next"_sv, nullptr), &mongoac_database_destroy);
    auto const db = db_owner.get();

    mongoac_database_drop(db, nullptr, nullptr, error);
    REQUIRE_MONGOAC_ERROR_OK(error);

    SECTION("null") {
        auto const doc = from_mongoac(mongoac_cursor_next(nullptr, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK(doc == nullptr);
    }

    SECTION("none") {
        auto const cursor_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_database_run_cursor_command(
                db,
                nullptr,
                to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"find": "coll"})"_bson, &bson_destroy).get()),
                nullptr,
                error),
            &mongoac_cursor_destroy);
        REQUIRE_MONGOAC_ERROR_OK(error);
        auto const cursor = cursor_owner.get();

        auto const done = from_mongoac(mongoac_cursor_next(cursor, error));
        CHECK_MONGOAC_ERROR_OK(error);
        auto const idempotency = from_mongoac(mongoac_cursor_next(cursor, error));
        CHECK_MONGOAC_ERROR_OK(error);

        CHECK(done == nullptr);
        CHECK(idempotency == nullptr);
    }

    SECTION("basic") {
        {
            auto const coll_owner = REQUIRE_MAKE_UNIQUE(
                mongoac_database_get_collection(db, "coll"_sv, nullptr), &mongoac_collection_destroy);
            auto const coll = coll_owner.get();
            auto const doc1 = REQUIRE_MAKE_UNIQUE(R"({"_id": 1})"_bson, &bson_destroy);
            auto const doc2 = REQUIRE_MAKE_UNIQUE(R"({"_id": 2})"_bson, &bson_destroy);

            (void)make_unique(
                mongoac_collection_insert_one(coll, nullptr, to_mongoac(doc1.get()), nullptr, error),
                &mongoac_insert_one_result_destroy);
            REQUIRE_MONGOAC_ERROR_OK(error);
            (void)make_unique(
                mongoac_collection_insert_one(coll, nullptr, to_mongoac(doc2.get()), nullptr, error),
                &mongoac_insert_one_result_destroy);
            REQUIRE_MONGOAC_ERROR_OK(error);
        }

        auto const cursor_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_database_run_cursor_command(
                db,
                nullptr,
                to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"find": "coll", "sort": {"_id": 1}})"_bson, &bson_destroy).get()),
                nullptr,
                error),
            &mongoac_cursor_destroy);
        REQUIRE_MONGOAC_ERROR_OK(error);
        auto const cursor = cursor_owner.get();

        auto const doc1_owner = from_mongoac(mongoac_cursor_next(cursor, error));
        CHECK_MONGOAC_ERROR_OK(error);
        auto const doc2_owner = from_mongoac(mongoac_cursor_next(cursor, error));
        CHECK_MONGOAC_ERROR_OK(error);
        auto const done = from_mongoac(mongoac_cursor_next(cursor, error));
        CHECK_MONGOAC_ERROR_OK(error);
        auto const idempotency = from_mongoac(mongoac_cursor_next(cursor, error));
        CHECK_MONGOAC_ERROR_OK(error);

        auto const doc1 = REQUIRE_BSON_STATIC(doc1_owner);
        auto const doc2 = REQUIRE_BSON_STATIC(doc2_owner);
        CHECK(done == nullptr);
        CHECK(idempotency == nullptr);

        auto const id1 = REQUIRE_BSON_ITER_FIND(doc1, "_id");
        auto const id2 = REQUIRE_BSON_ITER_FIND(doc2, "_id");

        CHECK(bson_iter_as_int64(&id1) == 1);
        CHECK(bson_iter_as_int64(&id2) == 2);
    }
}

TEST_CASE("next_async", "[mongoac][cursor]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
    auto const runtime = runtime_owner.get();
    auto const db_owner = REQUIRE_MAKE_UNIQUE(
        mongoac_client_get_database(client, "mongoac_cursor_next_async"_sv, nullptr), &mongoac_database_destroy);
    auto const db = db_owner.get();

    mongoac_database_drop(db, nullptr, nullptr, error);
    REQUIRE_MONGOAC_ERROR_OK(error);

    SECTION("null") {
        auto const future = make_unique(mongoac_cursor_next_async(nullptr), &mongoac_future_destroy);
        CHECK(future == nullptr);
    }

    SECTION("none") {
        auto const cursor_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_database_run_cursor_command(
                db,
                nullptr,
                to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"find": "coll"})"_bson, &bson_destroy).get()),
                nullptr,
                error),
            &mongoac_cursor_destroy);
        REQUIRE_MONGOAC_ERROR_OK(error);
        auto const cursor = cursor_owner.get();

        auto const f1 = REQUIRE_MAKE_UNIQUE(mongoac_cursor_next_async(cursor), &mongoac_future_destroy);
        auto const f2 = REQUIRE_MAKE_UNIQUE(mongoac_cursor_next_async(cursor), &mongoac_future_destroy);

        {
            std::array<mongoac_future_t*, 2u> futures = {{f1.get(), f2.get()}};
            mongoac_runtime_block_on_all(runtime, futures.data(), futures.size(), error);
            CHECK_MONGOAC_ERROR_OK(error);
        }

        auto const done = mongoac_future_get_optional_bson(f1.get(), error);
        CHECK_MONGOAC_ERROR_OK(error);
        auto const idempotency = mongoac_future_get_optional_bson(f2.get(), error);
        CHECK_MONGOAC_ERROR_OK(error);

        CHECK(done.ptr == nullptr);
        CHECK(idempotency.ptr == nullptr);
    }

    SECTION("basic") {
        {
            auto const coll_owner = REQUIRE_MAKE_UNIQUE(
                mongoac_database_get_collection(db, "coll"_sv, nullptr), &mongoac_collection_destroy);
            auto const coll = coll_owner.get();
            auto const doc1 = REQUIRE_MAKE_UNIQUE(R"({"_id": 1})"_bson, &bson_destroy);
            auto const doc2 = REQUIRE_MAKE_UNIQUE(R"({"_id": 2})"_bson, &bson_destroy);

            (void)make_unique(
                mongoac_collection_insert_one(coll, nullptr, to_mongoac(doc1.get()), nullptr, error),
                &mongoac_insert_one_result_destroy);
            REQUIRE_MONGOAC_ERROR_OK(error);
            (void)make_unique(
                mongoac_collection_insert_one(coll, nullptr, to_mongoac(doc2.get()), nullptr, error),
                &mongoac_insert_one_result_destroy);
            REQUIRE_MONGOAC_ERROR_OK(error);
        }

        auto const cursor_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_database_run_cursor_command(
                db,
                nullptr,
                to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"find": "coll", "sort": {"_id": 1}})"_bson, &bson_destroy).get()),
                nullptr,
                error),
            &mongoac_cursor_destroy);
        REQUIRE_MONGOAC_ERROR_OK(error);
        auto const cursor = cursor_owner.get();

        auto const f1 = REQUIRE_MAKE_UNIQUE(mongoac_cursor_next_async(cursor), &mongoac_future_destroy);
        auto const f2 = REQUIRE_MAKE_UNIQUE(mongoac_cursor_next_async(cursor), &mongoac_future_destroy);
        auto const f3 = REQUIRE_MAKE_UNIQUE(mongoac_cursor_next_async(cursor), &mongoac_future_destroy);
        auto const f4 = REQUIRE_MAKE_UNIQUE(mongoac_cursor_next_async(cursor), &mongoac_future_destroy);

        {
            std::array<mongoac_future_t*, 2u> futures = {{f1.get(), f2.get()}};
            mongoac_runtime_block_on_all(runtime, futures.data(), futures.size(), error);
            CHECK_MONGOAC_ERROR_OK(error);
        }

        {
            auto const doc1_view = mongoac_future_get_optional_bson(f1.get(), error);
            CHECK_MONGOAC_ERROR_OK(error);
            auto const doc2_view = mongoac_future_get_optional_bson(f2.get(), error);
            CHECK_MONGOAC_ERROR_OK(error);

            auto const doc1 = REQUIRE_BSON_STATIC(doc1_view);
            auto const doc2 = REQUIRE_BSON_STATIC(doc2_view);
            auto const id1 = REQUIRE_BSON_ITER_FIND(doc1, "_id");
            auto const id2 = REQUIRE_BSON_ITER_FIND(doc2, "_id");

            auto const actual = std::set<std::int64_t>({bson_iter_as_int64(&id1), bson_iter_as_int64(&id2)});
            auto const expected = std::set<std::int64_t>({1, 2});

            CHECK(actual == expected);
        }

        {
            std::array<mongoac_future_t*, 2u> futures = {{f3.get(), f4.get()}};
            mongoac_runtime_block_on_all(runtime, futures.data(), futures.size(), error);
            CHECK_MONGOAC_ERROR_OK(error);
        }

        {
            auto const done = mongoac_future_get_optional_bson(f3.get(), error);
            CHECK_MONGOAC_ERROR_OK(error);
            auto const idempotency = mongoac_future_get_optional_bson(f4.get(), error);
            CHECK_MONGOAC_ERROR_OK(error);

            CHECK(done.ptr == nullptr);
            CHECK(idempotency.ptr == nullptr);
        }
    }
}
