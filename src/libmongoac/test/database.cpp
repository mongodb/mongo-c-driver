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
#include <mongoac/database.h>

//

#include <mongoac/test/array.hh>
#include <mongoac/test/bson.hh>
#include <mongoac/test/client.hh>
#include <mongoac/test/error.hh>
#include <mongoac/test/memory.hh>
#include <mongoac/test/string.hh>

#include <mongoac/client.h>
#include <mongoac/collection.h>
#include <mongoac/cursor.h>
#include <mongoac/error.h>
#include <mongoac/future.h>
#include <mongoac/insert_one_result.h>
#include <mongoac/runtime.h>

#include <bson/bson.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

using mongoac::test::as_json;
using mongoac::test::doesnotexist_uri;
using mongoac::test::from_mongoac;
using mongoac::test::make_unique;
using mongoac::test::to_array;
using mongoac::test::operator""_sv;
using mongoac::test::operator""_bson;
using mongoac::test::default_uri;
using mongoac::test::to_bson;
using mongoac::test::to_mongoac;

TEST_CASE("destroy", "[mongoac][database]") {
    SECTION("null") {
        mongoac_database_destroy(nullptr);
        SUCCEED();
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

TEST_CASE("create_collection", "[mongoac][database]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const db_owner = REQUIRE_MAKE_UNIQUE(
        mongoac_client_get_database(client, "mongoac_database_create_collection"_sv, nullptr),
        &mongoac_database_destroy);
    auto const db = db_owner.get();

    // Start from a known state.
    mongoac_database_drop(db, nullptr, nullptr, error);
    REQUIRE_MONGOAC_ERROR_OK(error);

    SECTION("null db") {
        mongoac_database_create_collection(nullptr, nullptr, "coll"_sv, nullptr, error);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
    }

    SECTION("null name") {
        mongoac_database_create_collection(db, nullptr, {}, nullptr, error);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
    }

    SECTION("basic") {
        mongoac_database_create_collection(db, nullptr, "coll"_sv, nullptr, error);
        CHECK_MONGOAC_ERROR_OK(error);

        // Verify the collection exists via listCollections.
        auto const list = REQUIRE_MAKE_UNIQUE(R"({"listCollections": 1, "nameOnly": true})"_bson, &bson_destroy);
        auto const reply_owner =
            from_mongoac(mongoac_database_run_command(db, nullptr, to_mongoac(list.get()), nullptr, error));
        REQUIRE_MONGOAC_ERROR_OK(error);
        auto const reply = REQUIRE_BSON_STATIC(reply_owner);

        auto const cursor = REQUIRE_BSON_ITER_FIND(reply, "cursor");
        bson_iter_t batch = {};
        REQUIRE(bson_iter_recurse(&cursor, &batch));
        REQUIRE(bson_iter_find(&batch, "firstBatch"));
        bson_iter_t element = {};
        REQUIRE(bson_iter_recurse(&batch, &element));
        REQUIRE(bson_iter_next(&element));
        bson_iter_t coll = {};
        REQUIRE(bson_iter_recurse(&element, &coll));
        REQUIRE(bson_iter_find(&coll, "name"));
        CHECK(std::string_view(bson_iter_utf8(&coll, nullptr)) == "coll");
    }
}

TEST_CASE("create_collection_async", "[mongoac][database]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const db_owner = REQUIRE_MAKE_UNIQUE(
        mongoac_client_get_database(client, "mongoac_database_create_collection_async"_sv, nullptr),
        &mongoac_database_destroy);
    auto const db = db_owner.get();

    // Start from a known state.
    mongoac_database_drop(db, nullptr, nullptr, error);
    REQUIRE_MONGOAC_ERROR_OK(error);

    SECTION("null db") {
        auto const future = make_unique(
            mongoac_database_create_collection_async(nullptr, nullptr, "coll"_sv, nullptr), &mongoac_future_destroy);
        CHECK(future.get() == nullptr);
    }

    SECTION("null name") {
        auto const future =
            make_unique(mongoac_database_create_collection_async(db, nullptr, {}, nullptr), &mongoac_future_destroy);
        CHECK(future.get() == nullptr);
    }

    SECTION("basic") {
        auto const future_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_database_create_collection_async(db, nullptr, "coll"_sv, nullptr), &mongoac_future_destroy);
        auto const future = future_owner.get();

        {
            auto const runtime = REQUIRE_MAKE_UNIQUE(mongoac_database_get_runtime(db), &mongoac_runtime_destroy);
            mongoac_runtime_block_on(runtime.get(), future, error);
            CHECK_MONGOAC_ERROR_OK(error);
        }

        mongoac_future_get_void(future, error);
        CHECK_MONGOAC_ERROR_OK(error);

        // Verify the collection exists via listCollections.
        auto const list = REQUIRE_MAKE_UNIQUE(R"({"listCollections": 1, "nameOnly": true})"_bson, &bson_destroy);
        auto const reply_owner =
            from_mongoac(mongoac_database_run_command(db, nullptr, to_mongoac(list.get()), nullptr, error));
        REQUIRE_MONGOAC_ERROR_OK(error);
        auto const reply = REQUIRE_BSON_STATIC(reply_owner);

        auto const cursor = REQUIRE_BSON_ITER_FIND(reply, "cursor");
        bson_iter_t batch = {};
        REQUIRE(bson_iter_recurse(&cursor, &batch));
        REQUIRE(bson_iter_find(&batch, "firstBatch"));
        bson_iter_t element = {};
        REQUIRE(bson_iter_recurse(&batch, &element));
        REQUIRE(bson_iter_next(&element));
        bson_iter_t coll = {};
        REQUIRE(bson_iter_recurse(&element, &coll));
        REQUIRE(bson_iter_find(&coll, "name"));
        CHECK(std::string_view(bson_iter_utf8(&coll, nullptr)) == "coll");
    }
}

TEST_CASE("drop", "[mongoac][database]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();

    SECTION("null") {
        mongoac_database_drop(nullptr, nullptr, nullptr, error);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
    }

    SECTION("basic") {
        auto const client_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
        auto const client = client_owner.get();
        auto const db_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_get_database(client, "db"_sv, nullptr), &mongoac_database_destroy);
        auto const db = db_owner.get();

        mongoac_database_drop(db, nullptr, nullptr, error);
        CHECK_MONGOAC_ERROR_OK(error);
    }
}

TEST_CASE("drop_async", "[mongoac][database]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();

    SECTION("null") {
        auto const future =
            make_unique(mongoac_database_drop_async(nullptr, nullptr, nullptr), &mongoac_future_destroy);
        CHECK(future.get() == nullptr);
    }

    SECTION("basic") {
        auto const client_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
        auto const client = client_owner.get();
        auto const db_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_get_database(client, "db"_sv, nullptr), &mongoac_database_destroy);
        auto const db = db_owner.get();

        auto const future_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_database_drop_async(db, nullptr, nullptr), &mongoac_future_destroy);
        auto const future = future_owner.get();

        {
            auto const runtime = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
            mongoac_runtime_block_on(runtime.get(), future, error);
            CHECK_MONGOAC_ERROR_OK(error);
        }

        mongoac_future_get_void(future, error);
        CHECK_MONGOAC_ERROR_OK(error);
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

TEST_CASE("run_command", "[mongoac][database]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const db_owner = REQUIRE_MAKE_UNIQUE(
        mongoac_client_get_database(client, "mongoac_database_run_command"_sv, nullptr), &mongoac_database_destroy);
    auto const db = db_owner.get();

    mongoac_database_drop(db, nullptr, nullptr, error);
    REQUIRE_MONGOAC_ERROR_OK(error);

    SECTION("null db") {
        auto const reply = from_mongoac(mongoac_database_run_command(nullptr, nullptr, {}, nullptr, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK(reply == nullptr);
    }

    SECTION("null command") {
        auto const reply = from_mongoac(mongoac_database_run_command(db, nullptr, {}, nullptr, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK(reply == nullptr);
    }

    SECTION("invalid BSON command") {
        std::uint8_t const bytes = {};
        auto const reply = from_mongoac(mongoac_database_run_command(db, nullptr, {&bytes, 1u}, nullptr, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK(reply == nullptr);
    }

    SECTION("basic") {
        auto const ping = REQUIRE_MAKE_UNIQUE("{\"ping\": 1}"_bson, &bson_destroy);

        auto const reply_owner =
            from_mongoac(mongoac_database_run_command(db, nullptr, to_mongoac(ping.get()), nullptr, error));
        CHECK_MONGOAC_ERROR_OK(error);
        auto const reply = REQUIRE_BSON_STATIC(reply_owner);

        auto const iter = REQUIRE_BSON_ITER_FIND(reply, "ok");
        CHECK(bson_iter_as_bool(&iter));
    }

    SECTION("session") {
        auto const session_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_start_session(client, nullptr, error), &mongoac_client_session_destroy);
        auto const session = session_owner.get();

        {
            auto const coll_owner = REQUIRE_MAKE_UNIQUE(
                mongoac_database_get_collection(db, "coll"_sv, nullptr), &mongoac_collection_destroy);
            auto const coll = coll_owner.get();

            (void)REQUIRE_MAKE_UNIQUE(
                mongoac_collection_insert_one(
                    coll,
                    nullptr,
                    to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"_id": 1})"_bson, &bson_destroy).get()),
                    nullptr,
                    error),
                &mongoac_insert_one_result_destroy);
            REQUIRE_MONGOAC_ERROR_OK(error);
        }

        // Manually create a server-side session using a "find" command with `batchSize: 0`.
        auto const cursor_id = [&]() -> std::int64_t {
            auto const reply = from_mongoac(mongoac_database_run_command(
                db,
                session,
                to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"find": "coll", "batchSize": 0})"_bson, &bson_destroy).get()),
                nullptr,
                error));
            REQUIRE_MONGOAC_ERROR_OK(error);

            auto const doc = REQUIRE_BSON_STATIC(reply);
            auto const cursor_id = REQUIRE_BSON_ITER_FIND_DESCENDANT(doc, "cursor.id");
            auto const id = bson_iter_int64(&cursor_id);
            REQUIRE(id != 0);
            return id;
        }();

        auto const get_more =
            REQUIRE_MAKE_UNIQUE(BCON_NEW("getMore", BCON_INT64(cursor_id), "collection", "coll"), &bson_destroy);

        // Server should reject cursor "getMore" command without the associated session.
        {
            auto const reply =
                from_mongoac(mongoac_database_run_command(db, nullptr, to_mongoac(get_more.get()), nullptr, error));
            CHECK_MONGOAC_ERROR_CATEGORY(error, MONGOAC_ERROR_CATEGORY_SERVER);
            auto const msg = from_mongoac(mongoac_error_message(error));
            CHECK_THAT(
                msg,
                Catch::Matchers::ContainsSubstring("Cannot run getMore on cursor") &&
                    Catch::Matchers::ContainsSubstring("which was created in session"));
            CHECK(reply == nullptr);
        }

        // Server should accept cursor "getMore" command with the associated session.
        {
            auto const reply =
                from_mongoac(mongoac_database_run_command(db, session, to_mongoac(get_more.get()), nullptr, error));
            CHECK_MONGOAC_ERROR_OK(error);
            auto const doc = REQUIRE_BSON_STATIC(reply);
            auto const id = REQUIRE_BSON_ITER_FIND_DESCENDANT(doc, "cursor.nextBatch.0._id");
            CHECK(bson_iter_int32(&id) == 1);
        }
    }
}

TEST_CASE("run_command_async", "[mongoac][database]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
    auto const runtime = runtime_owner.get();
    auto const db_owner = REQUIRE_MAKE_UNIQUE(
        mongoac_client_get_database(client, "mongoac_database_run_command_async"_sv, nullptr),
        &mongoac_database_destroy);
    auto const db = db_owner.get();

    mongoac_database_drop(db, nullptr, nullptr, error);
    REQUIRE_MONGOAC_ERROR_OK(error);

    SECTION("null db") {
        auto const future =
            make_unique(mongoac_database_run_command_async(nullptr, nullptr, {}, nullptr), &mongoac_future_destroy);
        CHECK(future == nullptr);
    }

    SECTION("null command") {
        auto const future =
            make_unique(mongoac_database_run_command_async(db, nullptr, {}, nullptr), &mongoac_future_destroy);
        CHECK(future == nullptr);
    }

    SECTION("invalid BSON command") {
        std::uint8_t const bytes = {};
        auto const future = make_unique(
            mongoac_database_run_command_async(db, nullptr, {&bytes, 1u}, nullptr), &mongoac_future_destroy);
        CHECK(future == nullptr);
    }

    SECTION("basic") {
        auto const ping = REQUIRE_MAKE_UNIQUE(R"({"ping": 1})"_bson, &bson_destroy);

        auto const future_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_database_run_command_async(db, nullptr, to_mongoac(ping.get()), nullptr), &mongoac_future_destroy);
        auto const future = future_owner.get();

        mongoac_runtime_block_on(runtime, future, error);
        CHECK_MONGOAC_ERROR_OK(error);

        auto const reply_view = mongoac_future_get_bson(future, error);
        CHECK_MONGOAC_ERROR_OK(error);
        auto const reply = REQUIRE_BSON_STATIC(reply_view);

        auto const iter = REQUIRE_BSON_ITER_FIND(reply, "ok");
        CHECK(bson_iter_as_bool(&iter));
    }

    SECTION("session") {
        auto const session_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_start_session(client, nullptr, error), &mongoac_client_session_destroy);
        auto const session = session_owner.get();

        {
            auto const coll_owner = REQUIRE_MAKE_UNIQUE(
                mongoac_database_get_collection(db, "coll"_sv, nullptr), &mongoac_collection_destroy);
            auto const coll = coll_owner.get();

            (void)REQUIRE_MAKE_UNIQUE(
                mongoac_collection_insert_one(
                    coll,
                    nullptr,
                    to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"_id": 1})"_bson, &bson_destroy).get()),
                    nullptr,
                    error),
                &mongoac_insert_one_result_destroy);
            REQUIRE_MONGOAC_ERROR_OK(error);
        }

        // Manually create a server-side cursor using a "find" command with `batchSize: 0`.
        auto const cursor_id = [&]() -> std::int64_t {
            auto const future_owner = REQUIRE_MAKE_UNIQUE(
                mongoac_database_run_command_async(
                    db,
                    session,
                    to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"find": "coll", "batchSize": 0})"_bson, &bson_destroy).get()),
                    nullptr),
                &mongoac_future_destroy);
            auto const future = future_owner.get();
            mongoac_runtime_block_on(runtime, future, error);
            REQUIRE_MONGOAC_ERROR_OK(error);
            auto const reply = mongoac_future_get_bson(future, error);
            REQUIRE_MONGOAC_ERROR_OK(error);

            auto const doc = REQUIRE_BSON_STATIC(reply);
            auto const cursor_id = REQUIRE_BSON_ITER_FIND_DESCENDANT(doc, "cursor.id");
            auto const id = bson_iter_int64(&cursor_id);
            REQUIRE(id != 0);
            return id;
        }();

        auto const get_more =
            REQUIRE_MAKE_UNIQUE(BCON_NEW("getMore", BCON_INT64(cursor_id), "collection", "coll"), &bson_destroy);

        // Server should reject cursor "getMore" command without the associated session.
        {
            auto const missing_fut_owner = REQUIRE_MAKE_UNIQUE(
                mongoac_database_run_command_async(db, nullptr, to_mongoac(get_more.get()), nullptr),
                &mongoac_future_destroy);
            auto const missing_fut = missing_fut_owner.get();
            mongoac_runtime_block_on(runtime, missing_fut, error);
            REQUIRE_MONGOAC_ERROR_OK(error);

            auto const reply = mongoac_future_get_bson(missing_fut, error);
            CHECK_MONGOAC_ERROR_CATEGORY(error, MONGOAC_ERROR_CATEGORY_SERVER);
            auto const msg = from_mongoac(mongoac_error_message(error));
            CHECK_THAT(
                msg,
                Catch::Matchers::ContainsSubstring("Cannot run getMore on cursor") &&
                    Catch::Matchers::ContainsSubstring("which was created in session"));
            CHECK(reply.ptr == nullptr);
        }

        // Server should accept cursor "getMore" command with the associated session.
        {
            auto const session_fut_owner = REQUIRE_MAKE_UNIQUE(
                mongoac_database_run_command_async(db, session, to_mongoac(get_more.get()), nullptr),
                &mongoac_future_destroy);
            auto const session_fut = session_fut_owner.get();
            mongoac_runtime_block_on(runtime, session_fut, error);
            REQUIRE_MONGOAC_ERROR_OK(error);

            auto const reply = mongoac_future_get_bson(session_fut, error);
            CHECK_MONGOAC_ERROR_OK(error);
            auto const doc = REQUIRE_BSON_STATIC(reply);
            auto const id = REQUIRE_BSON_ITER_FIND_DESCENDANT(doc, "cursor.nextBatch.0._id");
            CHECK(bson_iter_int32(&id) == 1);
        }
    }
}

TEST_CASE("run_cursor_command", "[mongoac][database]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const db_owner = REQUIRE_MAKE_UNIQUE(
        mongoac_client_get_database(client, "mongoac_database_run_cursor_command"_sv, nullptr),
        &mongoac_database_destroy);
    auto const db = db_owner.get();

    mongoac_database_drop(db, nullptr, nullptr, error);
    REQUIRE_MONGOAC_ERROR_OK(error);

    SECTION("null db") {
        auto const cursor = make_unique(
            mongoac_database_run_cursor_command(nullptr, nullptr, {}, nullptr, error), &mongoac_cursor_destroy);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK(cursor == nullptr);
    }

    SECTION("null command") {
        auto const cursor =
            make_unique(mongoac_database_run_cursor_command(db, nullptr, {}, nullptr, error), &mongoac_cursor_destroy);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK(cursor == nullptr);
    }

    SECTION("invalid BSON command") {
        std::uint8_t const bytes[] = {0xff};
        auto const cursor = make_unique(
            mongoac_database_run_cursor_command(db, nullptr, {bytes, sizeof(bytes)}, nullptr, error),
            &mongoac_cursor_destroy);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK(cursor == nullptr);
    }

    SECTION("none") {
        auto const find = REQUIRE_MAKE_UNIQUE(R"({"find": "coll"})"_bson, &bson_destroy);

        auto const cursor_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_database_run_cursor_command(db, nullptr, to_mongoac(find.get()), nullptr, error),
            &mongoac_cursor_destroy);
        CHECK_MONGOAC_ERROR_OK(error);
        auto const cursor = cursor_owner.get();
        REQUIRE(cursor != nullptr);

        auto const doc_owner = from_mongoac(mongoac_cursor_next(cursor, error));
        CHECK_MONGOAC_ERROR_OK(error);
        CHECK(doc_owner == nullptr); // Zero documents is a valid result.
    }

    SECTION("some") {
        auto const coll_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_database_get_collection(db, "coll"_sv, nullptr), &mongoac_collection_destroy);
        auto const coll = coll_owner.get();

        {
            (void)make_unique(
                mongoac_collection_insert_one(
                    coll,
                    nullptr,
                    to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"_id": 1})"_bson, &bson_destroy).get()),
                    nullptr,
                    error),
                &mongoac_insert_one_result_destroy);
            REQUIRE_MONGOAC_ERROR_OK(error);
            (void)make_unique(
                mongoac_collection_insert_one(
                    coll,
                    nullptr,
                    to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"_id": 2})"_bson, &bson_destroy).get()),
                    nullptr,
                    error),
                &mongoac_insert_one_result_destroy);
            REQUIRE_MONGOAC_ERROR_OK(error);
        }

        auto const find = REQUIRE_MAKE_UNIQUE(
            R"({
                "find": "coll",
                "sort": {"_id": 1}
            })"_bson,
            &bson_destroy);

        auto const cursor_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_database_run_cursor_command(db, nullptr, to_mongoac(find.get()), nullptr, error),
            &mongoac_cursor_destroy);
        CHECK_MONGOAC_ERROR_OK(error);
        auto const cursor = cursor_owner.get();
        REQUIRE(cursor != nullptr);

        auto const doc1_owner = from_mongoac(mongoac_cursor_next(cursor, error));
        CHECK_MONGOAC_ERROR_OK(error);
        auto const doc2_owner = from_mongoac(mongoac_cursor_next(cursor, error));
        CHECK_MONGOAC_ERROR_OK(error);
        auto const done = from_mongoac(mongoac_cursor_next(cursor, error));
        CHECK_MONGOAC_ERROR_OK(error);
        auto const idempotency = from_mongoac(mongoac_cursor_next(cursor, error));
        CHECK_MONGOAC_ERROR_OK(error);

        CAPTURE(as_json(doc1_owner));
        CAPTURE(as_json(doc2_owner));
        CAPTURE(as_json(done));
        CAPTURE(as_json(idempotency));

        auto const doc1 = REQUIRE_BSON_STATIC(doc1_owner);
        auto const doc2 = REQUIRE_BSON_STATIC(doc2_owner);

        auto const id1 = REQUIRE_BSON_ITER_FIND(doc1, "_id");
        auto const id2 = REQUIRE_BSON_ITER_FIND(doc2, "_id");

        CHECK(bson_iter_int32(&id1) == 1);
        CHECK(bson_iter_int32(&id2) == 2);

        CHECK(done == nullptr);
        CHECK(idempotency == nullptr);
    }

    SECTION("session") {
        auto const session_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_start_session(client, nullptr, error), &mongoac_client_session_destroy);
        auto const session = session_owner.get();

        {
            auto const coll_owner = REQUIRE_MAKE_UNIQUE(
                mongoac_database_get_collection(db, "coll"_sv, nullptr), &mongoac_collection_destroy);
            auto const coll = coll_owner.get();

            (void)REQUIRE_MAKE_UNIQUE(
                mongoac_collection_insert_one(
                    coll,
                    nullptr,
                    to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"_id": 1})"_bson, &bson_destroy).get()),
                    nullptr,
                    error),
                &mongoac_insert_one_result_destroy);
            REQUIRE_MONGOAC_ERROR_OK(error);
        }

        // Manually create a server-side session using a "find" command with `batchSize: 0`.
        auto const find = REQUIRE_MAKE_UNIQUE(R"({"find": "coll", "batchSize": 0})"_bson, &bson_destroy);

        // Driver should reject a cursor command with a session from another client.
        {
            auto const other_client =
                REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
            auto const other_session = REQUIRE_MAKE_UNIQUE(
                mongoac_client_start_session(other_client.get(), nullptr, error), &mongoac_client_session_destroy);
            REQUIRE_MONGOAC_ERROR_OK(error);

            auto const reply = make_unique(
                mongoac_database_run_cursor_command(db, other_session.get(), to_mongoac(find.get()), nullptr, error),
                &mongoac_cursor_destroy);
            CHECK_MONGOAC_ERROR_CATEGORY(error, MONGOAC_ERROR_CATEGORY_RUST);
            auto const msg = from_mongoac(mongoac_error_message(error));
            CHECK_THAT(
                msg,
                Catch::Matchers::ContainsSubstring("the session provided to an operation") &&
                    Catch::Matchers::ContainsSubstring("must be created from the same client"));
            CHECK(reply == nullptr);
        }

        // Server should accept a cursor command with the associated session.
        {
            auto const cursor_owner = make_unique(
                mongoac_database_run_cursor_command(db, session, to_mongoac(find.get()), nullptr, error),
                &mongoac_cursor_destroy);
            CHECK_MONGOAC_ERROR_OK(error);
            auto const cursor = cursor_owner.get();
            REQUIRE(cursor != nullptr);

            // The empty first batch forces next() to send getMore with the original session.
            auto const reply = from_mongoac(mongoac_cursor_next(cursor, error));
            CHECK_MONGOAC_ERROR_OK(error);
            auto const doc = REQUIRE_BSON_STATIC(reply);
            auto const id = REQUIRE_BSON_ITER_FIND(doc, "_id");
            CHECK(bson_iter_int32(&id) == 1);
            auto const done = from_mongoac(mongoac_cursor_next(cursor, error));
            CHECK_MONGOAC_ERROR_OK(error);
            CHECK(done == nullptr);
        }
    }
}

TEST_CASE("run_cursor_command_async", "[mongoac][database]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
    auto const runtime = runtime_owner.get();
    auto const db_owner = REQUIRE_MAKE_UNIQUE(
        mongoac_client_get_database(client, "mongoac_database_run_cursor_command_async"_sv, nullptr),
        &mongoac_database_destroy);
    auto const db = db_owner.get();

    mongoac_database_drop(db, nullptr, nullptr, error);
    REQUIRE_MONGOAC_ERROR_OK(error);

    SECTION("null db") {
        auto const future = make_unique(
            mongoac_database_run_cursor_command_async(nullptr, nullptr, {}, nullptr), &mongoac_future_destroy);
        CHECK(future == nullptr);
    }

    SECTION("null command") {
        auto const future =
            make_unique(mongoac_database_run_cursor_command_async(db, nullptr, {}, nullptr), &mongoac_future_destroy);
        CHECK(future == nullptr);
    }

    SECTION("invalid BSON command") {
        std::uint8_t const bytes[] = {0xff};
        auto const future = make_unique(
            mongoac_database_run_cursor_command_async(db, nullptr, {bytes, sizeof(bytes)}, nullptr),
            &mongoac_future_destroy);
        CHECK(future == nullptr);
    }

    SECTION("none") {
        auto const find = REQUIRE_MAKE_UNIQUE(R"({"find": "coll"})"_bson, &bson_destroy);

        auto const future_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_database_run_cursor_command_async(db, nullptr, to_mongoac(find.get()), nullptr),
            &mongoac_future_destroy);
        auto const future = future_owner.get();

        mongoac_runtime_block_on(runtime, future, error);
        CHECK_MONGOAC_ERROR_OK(error);

        auto const cursor = mongoac_future_get_cursor(future, error);
        CHECK_MONGOAC_ERROR_OK(error);
        REQUIRE(cursor != nullptr);

        auto const owned_owner = REQUIRE_MAKE_UNIQUE(mongoac_cursor_clone(cursor), &mongoac_cursor_destroy);
        auto const owned = owned_owner.get();

        auto const doc_owner = from_mongoac(mongoac_cursor_next(owned, error));
        CHECK_MONGOAC_ERROR_OK(error);
        CHECK(doc_owner == nullptr);
    }

    SECTION("some") {
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
        REQUIRE_MONGOAC_ERROR_OK(error);
        (void)make_unique(
            mongoac_collection_insert_one(
                coll,
                nullptr,
                to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"_id": 2})"_bson, &bson_destroy).get()),
                nullptr,
                error),
            &mongoac_insert_one_result_destroy);
        REQUIRE_MONGOAC_ERROR_OK(error);

        auto const future_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_database_run_cursor_command_async(
                db,
                nullptr,
                to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"find": "coll","sort": {"_id": 1}})"_bson, &bson_destroy).get()),
                nullptr),
            &mongoac_future_destroy);
        auto const future = future_owner.get();

        mongoac_runtime_block_on(runtime, future, error);
        CHECK_MONGOAC_ERROR_OK(error);

        auto const cursor_view = mongoac_future_get_cursor(future, error);
        CHECK_MONGOAC_ERROR_OK(error);
        REQUIRE(cursor_view != nullptr);

        auto const cursor_owner = REQUIRE_MAKE_UNIQUE(mongoac_cursor_clone(cursor_view), &mongoac_cursor_destroy);
        auto const cursor = cursor_owner.get();

        auto const doc1_owner = from_mongoac(mongoac_cursor_next(cursor, error));
        CHECK_MONGOAC_ERROR_OK(error);
        auto const doc2_owner = from_mongoac(mongoac_cursor_next(cursor, error));
        CHECK_MONGOAC_ERROR_OK(error);
        auto const done = from_mongoac(mongoac_cursor_next(cursor, error));
        CHECK_MONGOAC_ERROR_OK(error);
        auto const idempotency = from_mongoac(mongoac_cursor_next(cursor, error));
        CHECK_MONGOAC_ERROR_OK(error);

        CAPTURE(as_json(doc1_owner));
        CAPTURE(as_json(doc2_owner));
        CAPTURE(as_json(done));
        CAPTURE(as_json(idempotency));

        auto const x1 = REQUIRE_BSON_STATIC(doc1_owner);
        auto const x2 = REQUIRE_BSON_STATIC(doc2_owner);

        auto const id1 = REQUIRE_BSON_ITER_FIND(x1, "_id");
        auto const id2 = REQUIRE_BSON_ITER_FIND(x2, "_id");

        CHECK(bson_iter_int32(&id1) == 1);
        CHECK(bson_iter_int32(&id2) == 2);

        CHECK(done == nullptr);
        CHECK(idempotency == nullptr);
    }

    SECTION("session") {
        auto const session_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_start_session(client, nullptr, error), &mongoac_client_session_destroy);
        auto const session = session_owner.get();

        {
            auto const coll_owner = REQUIRE_MAKE_UNIQUE(
                mongoac_database_get_collection(db, "coll"_sv, nullptr), &mongoac_collection_destroy);
            auto const coll = coll_owner.get();

            (void)REQUIRE_MAKE_UNIQUE(
                mongoac_collection_insert_one(
                    coll,
                    nullptr,
                    to_mongoac(REQUIRE_MAKE_UNIQUE(R"({"_id": 1})"_bson, &bson_destroy).get()),
                    nullptr,
                    error),
                &mongoac_insert_one_result_destroy);
            REQUIRE_MONGOAC_ERROR_OK(error);
        }

        // Manually create a server-side session using a "find" command with `batchSize: 0`.
        auto const find = REQUIRE_MAKE_UNIQUE(R"({"find": "coll", "batchSize": 0})"_bson, &bson_destroy);

        auto const other_client =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri(), nullptr), &mongoac_client_destroy);
        auto const other_session = REQUIRE_MAKE_UNIQUE(
            mongoac_client_start_session(other_client.get(), nullptr, error), &mongoac_client_session_destroy);
        REQUIRE_MONGOAC_ERROR_OK(error);

        auto const foreign_fut_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_database_run_cursor_command_async(db, other_session.get(), to_mongoac(find.get()), nullptr),
            &mongoac_future_destroy);
        auto const session_fut_owner = REQUIRE_MAKE_UNIQUE(
            mongoac_database_run_cursor_command_async(db, session, to_mongoac(find.get()), nullptr),
            &mongoac_future_destroy);

        auto const foreign_fut = foreign_fut_owner.get();
        auto const session_fut = session_fut_owner.get();

        {
            auto const futures = to_array<mongoac_future_t*>({foreign_fut, session_fut});
            mongoac_runtime_block_on_all(runtime, futures.data(), futures.size(), error);
            REQUIRE_MONGOAC_ERROR_OK(error);
        }

        // Driver should reject cursor iteration using an incorrect session.
        {
            auto const reply = mongoac_future_get_cursor(foreign_fut, error);
            CHECK_MONGOAC_ERROR_CATEGORY(error, MONGOAC_ERROR_CATEGORY_RUST);
            auto const msg = from_mongoac(mongoac_error_message(error));
            CHECK_THAT(
                msg,
                Catch::Matchers::ContainsSubstring("the session provided to an operation") &&
                    Catch::Matchers::ContainsSubstring("must be created from the same client"));
            CHECK(reply == nullptr);
        }

        // Server should accept cursor iteration using the correct session.
        {
            auto const cursor_view = mongoac_future_get_cursor(session_fut, error);
            CHECK_MONGOAC_ERROR_OK(error);
            auto const cursor_owner = REQUIRE_MAKE_UNIQUE(mongoac_cursor_clone(cursor_view), &mongoac_cursor_destroy);
            auto const cursor = cursor_owner.get();

            auto const reply = from_mongoac(mongoac_cursor_next(cursor, error));
            CHECK_MONGOAC_ERROR_OK(error);
            auto const doc = REQUIRE_BSON_STATIC(reply);
            auto const id = REQUIRE_BSON_ITER_FIND(doc, "_id");
            CHECK(bson_iter_int32(&id) == 1);

            auto const done = from_mongoac(mongoac_cursor_next(cursor, error));
            CHECK_MONGOAC_ERROR_OK(error);
            CHECK(done == nullptr);
        }
    }
}
