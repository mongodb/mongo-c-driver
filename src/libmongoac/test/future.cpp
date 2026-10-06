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

#include <mongoac/future.h>

//

#include <mongoac/test/client.hh>
#include <mongoac/test/error.hh>
#include <mongoac/test/memory.hh>
#include <mongoac/test/string.hh>

#include <mongoac/client.h>
#include <mongoac/error.h>
#include <mongoac/runtime.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>

using mongoac::test::doesnotexist_uri;
using mongoac::test::from_mongoac;
using mongoac::test::make_unique;

TEST_CASE("destroy", "[mongoac][future]") {
    SECTION("null") {
        CHECK_NOTHROW(mongoac_future_destroy(nullptr));
    }
}

TEST_CASE("clone", "[mongoac][future]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();

    SECTION("null") {
        auto const copy = make_unique(mongoac_future_clone(nullptr), &mongoac_future_destroy);
        CHECK(copy == nullptr);
    }

    SECTION("basic") {
        auto const orig_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const orig = orig_owner.get();
        auto const copy_owner = REQUIRE_MAKE_UNIQUE(mongoac_future_clone(orig), &mongoac_future_destroy);
        auto const copy = copy_owner.get();

        CHECK(orig != copy);

        CHECK_FALSE(mongoac_future_is_ready(orig));
        CHECK_FALSE(mongoac_future_is_ready(copy));

        // Same associated runtime.
        auto const runtime = REQUIRE_MAKE_UNIQUE(mongoac_future_get_runtime(orig), &mongoac_runtime_destroy);
        CHECK(mongoac_runtime_address(runtime.get()) == mongoac_future_get_runtime_address(orig));
        CHECK(mongoac_runtime_address(runtime.get()) == mongoac_future_get_runtime_address(copy));

        // Progressing copy also progresses original (shared state).
        mongoac_runtime_block_on(runtime.get(), copy, error);
        CHECK_MONGOAC_ERROR_OK(error);

        // Same shared state.
        CHECK(mongoac_future_is_ready(orig));
        CHECK(mongoac_future_is_ready(copy));
    }
}

TEST_CASE("get_runtime", "[mongoac][future]") {
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();

    SECTION("null") {
        auto const runtime = make_unique(mongoac_future_get_runtime(nullptr), &mongoac_runtime_destroy);
        CHECK(runtime.get() == nullptr);
    }

    SECTION("basic") {
        auto const future = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);

        auto const runtime = make_unique(mongoac_future_get_runtime(future.get()), &mongoac_runtime_destroy);
        CHECK(runtime.get() != nullptr);
    }

    SECTION("unique") {
        auto const other_client_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const other_client = other_client_owner.get();

        auto const f1 = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const r1 = REQUIRE_MAKE_UNIQUE(mongoac_future_get_runtime(f1.get()), &mongoac_runtime_destroy);
        auto const f2 = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(other_client), &mongoac_future_destroy);
        auto const r2 = REQUIRE_MAKE_UNIQUE(mongoac_future_get_runtime(f2.get()), &mongoac_runtime_destroy);

        CHECK(mongoac_runtime_address(r1.get()) != mongoac_runtime_address(r2.get()));
    }

    SECTION("shared") {
        auto const f1 = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const r1 = REQUIRE_MAKE_UNIQUE(mongoac_future_get_runtime(f1.get()), &mongoac_runtime_destroy);
        auto const f2 = REQUIRE_MAKE_UNIQUE(mongoac_future_clone(f1.get()), &mongoac_future_destroy);

        auto const r2 = REQUIRE_MAKE_UNIQUE(mongoac_future_get_runtime(f2.get()), &mongoac_runtime_destroy);
        CHECK(mongoac_runtime_address(r1.get()) == mongoac_runtime_address(r2.get()));
    }
}

TEST_CASE("get_runtime_address", "[mongoac][future]") {
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();

    SECTION("null") {
        CHECK(mongoac_future_get_runtime_address(nullptr) == 0u);
    }

    SECTION("basic") {
        auto const future = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);

        CHECK(mongoac_future_get_runtime_address(future.get()) != 0u);
    }

    SECTION("unique") {
        auto const other_client_owner =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const other_client = other_client_owner.get();

        auto const f1 = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f2 = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(other_client), &mongoac_future_destroy);

        auto const a1 = mongoac_future_get_runtime_address(f1.get());
        auto const a2 = mongoac_future_get_runtime_address(f2.get());

        CHECK(a1 != a2);
    }

    SECTION("shared") {
        auto const f1 = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f2 = REQUIRE_MAKE_UNIQUE(mongoac_future_clone(f1.get()), &mongoac_future_destroy);

        auto const a1 = mongoac_future_get_runtime_address(f1.get());
        auto const a2 = mongoac_future_get_runtime_address(f2.get());

        CHECK(a1 == a2);
    }
}

TEST_CASE("is_ready", "[mongoac][future]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
    auto const runtime = runtime_owner.get();

    SECTION("null") {
        CHECK_FALSE(mongoac_future_is_ready(nullptr));
    }

    SECTION("block on") {
        auto const future_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const future = future_owner.get();
        CHECK_FALSE(mongoac_future_is_ready(future));

        // Block-on progress functions are also polling functions.
        mongoac_runtime_block_on(runtime, future, error);
        CHECK_MONGOAC_ERROR_OK(error);

        // Readiness is a block-on postcondition on success.
        CHECK(mongoac_future_is_ready(future));
        CHECK_NOTHROW(mongoac_future_get_void(future, error));
        CHECK_MONGOAC_ERROR_OK(error);
    }

    SECTION("make progress") {
        auto const future_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const future = future_owner.get();
        CHECK_FALSE(mongoac_future_is_ready(future));

        // Non-polling progress function do not complete the future.
        mongoac_runtime_make_progress_for(runtime, 0);
        CHECK_FALSE(mongoac_future_is_ready(future));

        // `is_ready()` is not a polling function: it cannot complete the future.
        if (mongoac_future_poll(future)) {
            CHECK(mongoac_future_is_ready(future));
        } else {
            CHECK_FALSE(mongoac_future_is_ready(future));

            // Defensive measure in case the shutdown op did not complete in a single pass.
            mongoac_runtime_block_on(runtime, future, error);
            CHECK_MONGOAC_ERROR_OK(error);

            CHECK(mongoac_future_is_ready(future));
        }

        CHECK_NOTHROW(mongoac_future_get_void(future, error));
        CHECK_MONGOAC_ERROR_OK(error);
    }
}

TEST_CASE("getters", "[mongoac][future]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
    auto const runtime = runtime_owner.get();
    auto const future_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
    auto const future = future_owner.get();

    SECTION("null") {
        CHECK_NOTHROW(mongoac_future_get_void(nullptr, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "future: must not be null");
    }

    SECTION("not ready") {
        CHECK_NOTHROW(mongoac_future_get_void(future, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "future is not ready");
    }

    SECTION("type mismatch: bool") {
        CHECK_NOTHROW(mongoac_runtime_block_on(runtime, future, error));
        CHECK_MONGOAC_ERROR_OK(error);

        CHECK(mongoac_future_get_bool(future, error) == bool{}); // Default::default()
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "future does not return a bool");
    }

    SECTION("type mismatch: uint64") {
        CHECK_NOTHROW(mongoac_runtime_block_on(runtime, future, error));
        CHECK_MONGOAC_ERROR_OK(error);

        CHECK(mongoac_future_get_uint64(future, error) == uint64_t{}); // Default::default()
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "future does not return a uint64");
    }

    SECTION("basic") {
        CHECK_NOTHROW(mongoac_runtime_block_on(runtime, future, error));
        CHECK_MONGOAC_ERROR_OK(error);

        CHECK_NOTHROW(mongoac_future_get_void(future, error));
        CHECK_MONGOAC_ERROR_OK(error);
    }
}

TEST_CASE("poll", "[mongoac][future]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
    auto const runtime = runtime_owner.get();

    SECTION("null") {
        CHECK_FALSE(mongoac_future_poll(nullptr));
    }

    SECTION("block on") {
        auto const future_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const future = future_owner.get();
        CHECK_FALSE(mongoac_future_poll(future));

        // Block-on progress functions are also polling functions.
        mongoac_runtime_block_on(runtime, future, error);
        CHECK_MONGOAC_ERROR_OK(error);

        // Polling a ready future is not an error.
        CHECK(mongoac_future_poll(future));
        CHECK_NOTHROW(mongoac_future_get_void(future, error));
        CHECK_MONGOAC_ERROR_OK(error);
    }

    SECTION("make progress") {
        auto const future_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const future = future_owner.get();
        CHECK_FALSE(mongoac_future_poll(future));

        // Eventual completion is guaranteed.
        for (int count = 0; !mongoac_future_poll(future); ++count) {
            // Defensive measure in case the shutdown op did not complete in a single pass.
            if (count >= 10) {
                FAIL("client shutdown did not complete within 10 iterations");
            }
            mongoac_runtime_make_progress(runtime);
        }

        CHECK(mongoac_future_is_ready(future));
        CHECK_NOTHROW(mongoac_future_get_void(future, error));
        CHECK_MONGOAC_ERROR_OK(error);
    }
}
