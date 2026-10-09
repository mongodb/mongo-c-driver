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

#include <mongoac/runtime.h>

//

#include <mongoac/test/client.hh>
#include <mongoac/test/error.hh>
#include <mongoac/test/memory.hh>
#include <mongoac/test/string.hh>

#include <mongoac/client.h>
#include <mongoac/error.h>
#include <mongoac/future.h>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iterator>

using mongoac::test::doesnotexist_uri;
using mongoac::test::from_mongoac;
using mongoac::test::make_unique;
using mongoac::test::operator""_sv;

TEST_CASE("destroy", "[mongoac][runtime]") {
    SECTION("null") {
        CHECK_NOTHROW(mongoac_runtime_destroy(nullptr));
    }

    SECTION("client") {
        auto client = REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto runtime = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client.get()), &mongoac_runtime_destroy);

        // Destroy the associated client.
        CHECK_NOTHROW(client.reset());

        // Runtime may outlive its associated client.
        CHECK(mongoac_runtime_address(runtime.get()) != 0u);

        // Destroy the runtime after its associated client.
        CHECK_NOTHROW(runtime.reset());
    }
}

TEST_CASE("clone", "[mongoac][runtime]") {
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();

    SECTION("null") {
        auto const copy = make_unique(mongoac_runtime_clone(nullptr), &mongoac_runtime_destroy);
        CHECK(copy == nullptr);
    }

    SECTION("basic") {
        auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
        auto const runtime = runtime_owner.get();

        auto const copy_owner = REQUIRE_MAKE_UNIQUE(mongoac_runtime_clone(runtime), &mongoac_runtime_destroy);
        auto const copy = copy_owner.get();
        CHECK(copy != nullptr);

        CHECK(runtime != copy);
        CHECK(mongoac_runtime_address(runtime) == mongoac_runtime_address(copy));
    }
}

TEST_CASE("address", "[mongoac][runtime]") {
    SECTION("null") {
        CHECK(mongoac_runtime_address(nullptr) == 0u);
    }

    SECTION("basic") {
        auto const client =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const runtime = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client.get()), &mongoac_runtime_destroy);

        CHECK(mongoac_runtime_address(runtime.get()) != 0u);
    }

    SECTION("unique") {
        auto const c1 = REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const r1 = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(c1.get()), &mongoac_runtime_destroy);
        auto const c2 = REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
        auto const r2 = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(c2.get()), &mongoac_runtime_destroy);

        CHECK(mongoac_runtime_address(r1.get()) != mongoac_runtime_address(r2.get()));
    }

    SECTION("shared") {
        auto const client =
            REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);

        auto const r1 = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client.get()), &mongoac_runtime_destroy);
        auto const r2 = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client.get()), &mongoac_runtime_destroy);

        CHECK(mongoac_runtime_address(r1.get()) == mongoac_runtime_address(r2.get()));
    }
}

TEST_CASE("block_on", "[mongoac][runtime]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
    auto const runtime = runtime_owner.get();

    SECTION("null runtime") {
        CHECK_NOTHROW(mongoac_runtime_block_on(nullptr, nullptr, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "runtime: must not be null");
    }

    SECTION("null future") {
        CHECK_NOTHROW(mongoac_runtime_block_on(runtime, nullptr, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "future: must not be null");
    }

    SECTION("basic") {
        auto const future_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const future = future_owner.get();

        CHECK_FALSE(mongoac_future_is_ready(future));
        auto const t1 = std::chrono::steady_clock::now();
        CHECK_NOTHROW(mongoac_runtime_block_on(runtime, future, error));
        auto const t2 = std::chrono::steady_clock::now();
        CHECK_MONGOAC_ERROR_OK(error);
        CHECK(mongoac_future_is_ready(future));
        auto const d1 = t2 - t1;

        // Idempotence.
        auto const t3 = std::chrono::steady_clock::now();
        CHECK_NOTHROW(mongoac_runtime_block_on(runtime, future, error));
        auto const t4 = std::chrono::steady_clock::now();
        CHECK_MONGOAC_ERROR_OK(error);
        auto const d2 = t4 - t3;

        // Early-return given a ready future.
        CHECK(d2 <= d1);
    }
}

TEST_CASE("block_on_with_timeout", "[mongoac][runtime]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
    auto const runtime = runtime_owner.get();

    SECTION("null runtime") {
        CHECK_NOTHROW(mongoac_runtime_block_on_with_timeout(nullptr, nullptr, 0u, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "runtime: must not be null");
    }

    SECTION("null future") {
        CHECK_NOTHROW(mongoac_runtime_block_on_with_timeout(runtime, nullptr, 0u, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "future: must not be null");
    }

    SECTION("timeout") {
        SUCCEED("TODO: use a long-running command to reliably trigger a timeout error");
    }

    SECTION("basic") {
        auto const future_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const future = future_owner.get();

        CHECK_FALSE(mongoac_future_is_ready(future));

        auto const t1 = std::chrono::steady_clock::now();
        CHECK_NOTHROW(mongoac_runtime_block_on_with_timeout(runtime, future, 0u, error));
        auto const t2 = std::chrono::steady_clock::now();
        CHECK_MONGOAC_ERROR_OK(error);
        CHECK(mongoac_future_is_ready(future));
        auto const d1 = t2 - t1;

        // Idempotence.
        auto const t3 = std::chrono::steady_clock::now();
        CHECK_NOTHROW(mongoac_runtime_block_on_with_timeout(runtime, future, 0u, error));
        auto const t4 = std::chrono::steady_clock::now();
        CHECK_MONGOAC_ERROR_OK(error);
        auto const d2 = t4 - t3;

        // Early-return given a ready future.
        CHECK(d2 <= d1);
    }
}

TEST_CASE("block_on_any", "[mongoac][runtime]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
    auto const runtime = runtime_owner.get();

    SECTION("null runtime") {
        CHECK(mongoac_runtime_block_on_any(nullptr, nullptr, 0u, error) == nullptr);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "runtime: must not be null");
    }

    SECTION("null futures") {
        CHECK(mongoac_runtime_block_on_any(runtime, nullptr, 0u, error) == nullptr);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array is null");
    }

    SECTION("zero futures") {
        auto const f1 = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);

        mongoac_future_t* const futures[] = {f1.get()};

        CHECK(mongoac_runtime_block_on_any(runtime, futures, 0u, error) == nullptr);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array is empty");
    }

    SECTION("null element") {
        auto const f1_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f1 = f1_owner.get();
        auto const f2_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f2 = f2_owner.get();

        SECTION("0") {
            mongoac_future_t* const futures[] = {nullptr, f1, f2};

            CHECK(mongoac_runtime_block_on_any(runtime, futures, 3u, error) == nullptr);
            CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
            CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array element at index 0: must not be null");
        }

        SECTION("1") {
            mongoac_future_t* const futures[] = {f1, nullptr, f2};

            CHECK(mongoac_runtime_block_on_any(runtime, futures, 3u, error) == nullptr);
            CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
            CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array element at index 1: must not be null");
        }

        SECTION("2") {
            mongoac_future_t* const futures[] = {f1, f2, nullptr};

            CHECK(mongoac_runtime_block_on_any(runtime, futures, 3u, error) == nullptr);
            CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
            CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array element at index 2: must not be null");
        }
    }

    SECTION("basic") {
        auto const f1_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f1 = f1_owner.get();
        auto const f2_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f2 = f2_owner.get();

        mongoac_future_t* const futures[] = {f1, f2};

        auto const t1 = std::chrono::steady_clock::now();
        auto const iter = mongoac_runtime_block_on_any(runtime, futures, 2u, error);
        auto const t2 = std::chrono::steady_clock::now();
        REQUIRE_MONGOAC_ERROR_OK(error);
        auto const d1 = t2 - t1;

        // When not null, the iterator must always point to an element in the futures array.
        CAPTURE(futures, *iter);
        REQUIRE(std::find(std::begin(futures), std::end(futures), *iter) != std::end(futures));
        auto const ready = (iter == &futures[0] ? f1 : f2);
        auto const pending = (iter == &futures[0] ? f2 : f1);

        // Only one future completes at a time.
        CHECK(mongoac_future_is_ready(ready));
        CHECK_FALSE(mongoac_future_is_ready(pending));

        // Idempotence.
        auto const t3 = std::chrono::steady_clock::now();
        auto const iter2 = mongoac_runtime_block_on_any(runtime, futures, 2u, error);
        auto const t4 = std::chrono::steady_clock::now();
        REQUIRE_MONGOAC_ERROR_OK(error);
        CHECK(iter == iter2);
        auto const d2 = t4 - t3;

        // Early-return given any future is ready.
        CHECK(d2 <= d1);
    }
}

TEST_CASE("block_on_any_with_timeout", "[mongoac][runtime]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
    auto const runtime = runtime_owner.get();

    SECTION("null runtime") {
        CHECK(mongoac_runtime_block_on_any_with_timeout(nullptr, nullptr, 0u, 0u, error) == nullptr);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "runtime: must not be null");
    }

    SECTION("null futures") {
        CHECK(mongoac_runtime_block_on_any_with_timeout(runtime, nullptr, 0u, 0u, error) == nullptr);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array is null");
    }

    SECTION("zero futures") {
        auto const f1 = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);

        mongoac_future_t* const futures[] = {f1.get()};

        CHECK(mongoac_runtime_block_on_any_with_timeout(runtime, futures, 0u, 0u, error) == nullptr);
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array is empty");
    }

    SECTION("timeout") {
        SUCCEED("TODO: use a long-running command to reliably trigger a timeout error");
    }

    SECTION("null element") {
        auto const f1_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f1 = f1_owner.get();
        auto const f2_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f2 = f2_owner.get();

        SECTION("0") {
            mongoac_future_t* const futures[] = {nullptr, f1, f2};

            CHECK(mongoac_runtime_block_on_any_with_timeout(runtime, futures, 3u, 0u, error) == nullptr);
            CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
            CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array element at index 0: must not be null");
        }

        SECTION("1") {
            mongoac_future_t* const futures[] = {f1, nullptr, f2};

            CHECK(mongoac_runtime_block_on_any_with_timeout(runtime, futures, 3u, 0u, error) == nullptr);
            CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
            CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array element at index 1: must not be null");
        }

        SECTION("2") {
            mongoac_future_t* const futures[] = {f1, f2, nullptr};

            CHECK(mongoac_runtime_block_on_any_with_timeout(runtime, futures, 3u, 0u, error) == nullptr);
            CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
            CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array element at index 2: must not be null");
        }
    }

    SECTION("basic") {
        auto const f1_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f1 = f1_owner.get();
        auto const f2_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f2 = f2_owner.get();

        mongoac_future_t* const futures[] = {f1, f2};

        auto const t1 = std::chrono::steady_clock::now();
        auto const iter = mongoac_runtime_block_on_any_with_timeout(runtime, futures, 2u, 0u, error);
        auto const t2 = std::chrono::steady_clock::now();
        REQUIRE_MONGOAC_ERROR_OK(error);
        auto const d1 = t2 - t1;

        // When not null, the iterator must always point to an element in the futures array.
        CAPTURE(futures, *iter);
        REQUIRE(std::find(std::begin(futures), std::end(futures), *iter) != std::end(futures));
        auto const ready = (iter == &futures[0] ? f1 : f2);
        auto const pending = (iter == &futures[0] ? f2 : f1);

        // Only one future completes at a time.
        CHECK(mongoac_future_is_ready(ready));
        CHECK_FALSE(mongoac_future_is_ready(pending));

        // Idempotence.
        auto const t3 = std::chrono::steady_clock::now();
        auto const iter2 = mongoac_runtime_block_on_any_with_timeout(runtime, futures, 2u, 0u, error);
        auto const t4 = std::chrono::steady_clock::now();
        REQUIRE_MONGOAC_ERROR_OK(error);
        CHECK(iter == iter2);
        auto const d2 = t4 - t3;

        // Early-return given any future is ready.
        CHECK(d2 <= d1);
    }
}

TEST_CASE("block_on_all", "[mongoac][runtime]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
    auto const runtime = runtime_owner.get();

    SECTION("null runtime") {
        CHECK_NOTHROW(mongoac_runtime_block_on_all(nullptr, nullptr, 0u, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "runtime: must not be null");
    }

    SECTION("null futures") {
        CHECK_NOTHROW(mongoac_runtime_block_on_all(runtime, nullptr, 0u, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array is null");
    }

    SECTION("zero futures") {
        auto const f1 = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);

        mongoac_future_t* const futures[] = {f1.get()};

        CHECK_NOTHROW(mongoac_runtime_block_on_all(runtime, futures, 0u, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array is empty");
    }

    SECTION("null element") {
        auto const f1_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f1 = f1_owner.get();
        auto const f2_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f2 = f2_owner.get();

        SECTION("0") {
            mongoac_future_t* const futures[] = {nullptr, f1, f2};

            CHECK_NOTHROW(mongoac_runtime_block_on_all(runtime, futures, 3u, error));
            CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
            CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array element at index 0: must not be null");
        }

        SECTION("1") {
            mongoac_future_t* const futures[] = {f1, nullptr, f2};

            CHECK_NOTHROW(mongoac_runtime_block_on_all(runtime, futures, 3u, error));
            CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
            CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array element at index 1: must not be null");
        }

        SECTION("2") {
            mongoac_future_t* const futures[] = {f1, f2, nullptr};

            CHECK_NOTHROW(mongoac_runtime_block_on_all(runtime, futures, 3u, error));
            CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
            CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array element at index 2: must not be null");
        }
    }

    SECTION("basic") {
        auto const f1_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f1 = f1_owner.get();
        auto const f2_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f2 = f2_owner.get();

        CHECK_FALSE(mongoac_future_is_ready(f1));
        CHECK_FALSE(mongoac_future_is_ready(f2));

        mongoac_future_t* const futures[] = {f1, f2};

        // Block on the first element only.
        auto const t1 = std::chrono::steady_clock::now();
        CHECK_NOTHROW(mongoac_runtime_block_on_all(runtime, futures, 1u, error));
        auto const t2 = std::chrono::steady_clock::now();
        CHECK_MONGOAC_ERROR_OK(error);
        CHECK(mongoac_future_is_ready(f1));
        auto const d1 = t2 - t1;

        // Idempotence.
        auto const t3 = std::chrono::steady_clock::now();
        CHECK_NOTHROW(mongoac_runtime_block_on_all(runtime, futures, 1u, error));
        auto const t4 = std::chrono::steady_clock::now();
        CHECK_MONGOAC_ERROR_OK(error);
        auto const d2 = t4 - t3;

        // Early-return given all futures are ready.
        CHECK(d2 <= d1);

        // Elements may be a mix of ready and pending futures.
        CHECK_NOTHROW(mongoac_runtime_block_on_all(runtime, futures, 2u, error));
        CHECK_MONGOAC_ERROR_OK(error);
        CHECK(mongoac_future_is_ready(f2));
        // Do not benchmark this block-on: due to two shutdown operations, the second shutdown may complete sooner than
        // expected (with an error). Cannot reliably compare its duration against either the first shutdown operation or
        // the early-return.

        // Idempotence.
        auto const t5 = std::chrono::steady_clock::now();
        CHECK_NOTHROW(mongoac_runtime_block_on_all(runtime, futures, 2u, error));
        auto const t6 = std::chrono::steady_clock::now();
        CHECK_MONGOAC_ERROR_OK(error);
        auto const d3 = t6 - t5;

        // Early-return given all futures are ready.
        CHECK(d3 <= d1);
    }
}

TEST_CASE("block_on_all_with_timeout", "[mongoac][runtime]") {
    auto const error_owner = REQUIRE_MAKE_UNIQUE(mongoac_error_new(), &mongoac_error_destroy);
    auto const error = error_owner.get();
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
    auto const runtime = runtime_owner.get();

    SECTION("null runtime") {
        CHECK_NOTHROW(mongoac_runtime_block_on_all_with_timeout(nullptr, nullptr, 0u, 0u, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "runtime: must not be null");
    }

    SECTION("null futures") {
        CHECK_NOTHROW(mongoac_runtime_block_on_all_with_timeout(runtime, nullptr, 0u, 0u, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array is null");
    }

    SECTION("zero futures") {
        auto const f1 = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);

        mongoac_future_t* const futures[] = {f1.get()};

        CHECK_NOTHROW(mongoac_runtime_block_on_all_with_timeout(runtime, futures, 0u, 0u, error));
        CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
        CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array is empty");
    }

    SECTION("null element") {
        auto const f1_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f1 = f1_owner.get();
        auto const f2_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f2 = f2_owner.get();

        SECTION("0") {
            mongoac_future_t* const futures[] = {nullptr, f1, f2};

            CHECK_NOTHROW(mongoac_runtime_block_on_all_with_timeout(runtime, futures, 3u, 0u, error));
            CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
            CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array element at index 0: must not be null");
        }

        SECTION("1") {
            mongoac_future_t* const futures[] = {f1, nullptr, f2};

            CHECK_NOTHROW(mongoac_runtime_block_on_all_with_timeout(runtime, futures, 3u, 0u, error));
            CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
            CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array element at index 1: must not be null");
        }

        SECTION("2") {
            mongoac_future_t* const futures[] = {f1, f2, nullptr};

            CHECK_NOTHROW(mongoac_runtime_block_on_all_with_timeout(runtime, futures, 3u, 0u, error));
            CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error);
            CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, "futures array element at index 2: must not be null");
        }
    }

    SECTION("timeout") {
        SUCCEED("TODO: use a long-running command to reliably trigger a timeout error");
    }

    SECTION("basic") {
        auto const f1_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f1 = f1_owner.get();
        auto const f2_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const f2 = f2_owner.get();

        CHECK_FALSE(mongoac_future_is_ready(f1));
        CHECK_FALSE(mongoac_future_is_ready(f2));

        mongoac_future_t* const futures[] = {f1, f2};

        // Block on the first element only.
        auto const t1 = std::chrono::steady_clock::now();
        CHECK_NOTHROW(mongoac_runtime_block_on_all_with_timeout(runtime, futures, 1u, 0u, error));
        auto const t2 = std::chrono::steady_clock::now();
        CHECK_MONGOAC_ERROR_OK(error);
        CHECK(mongoac_future_is_ready(f1));
        auto const d1 = t2 - t1;

        // Idempotence.
        auto const t3 = std::chrono::steady_clock::now();
        CHECK_NOTHROW(mongoac_runtime_block_on_all_with_timeout(runtime, futures, 1u, 0u, error));
        auto const t4 = std::chrono::steady_clock::now();
        CHECK_MONGOAC_ERROR_OK(error);
        auto const d2 = t4 - t3;

        // Early-return given all futures are ready.
        CHECK(d2 <= d1);

        // Elements may be a mix of ready and pending futures.
        CHECK_NOTHROW(mongoac_runtime_block_on_all_with_timeout(runtime, futures, 2u, 0u, error));
        CHECK_MONGOAC_ERROR_OK(error);
        CHECK(mongoac_future_is_ready(f2));
        // Do not benchmark this block-on: due to double-shutdown, the second future may complete sooner than expected.

        // Idempotence.
        auto const t5 = std::chrono::steady_clock::now();
        CHECK_NOTHROW(mongoac_runtime_block_on_all_with_timeout(runtime, futures, 2u, 0u, error));
        auto const t6 = std::chrono::steady_clock::now();
        CHECK_MONGOAC_ERROR_OK(error);
        auto const d3 = t6 - t5;

        // Early-return given all futures are ready.
        CHECK(d3 <= d1);
    }
}

TEST_CASE("make_progress", "[mongoac][runtime]") {
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
    auto const runtime = runtime_owner.get();

    SECTION("null") {
        CHECK_NOTHROW(mongoac_runtime_make_progress(nullptr));
    }

    SECTION("basic") {
        auto const future_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_shutdown_async(client), &mongoac_future_destroy);
        auto const future = future_owner.get();
        CHECK_FALSE(mongoac_future_poll(future));

        auto const t1 = std::chrono::steady_clock::now();
        CHECK_NOTHROW(mongoac_runtime_make_progress(runtime));
        auto const t2 = std::chrono::steady_clock::now();
        auto const d1 = t2 - t1;

        // This test (and others which use `shutdown_async()`) assumes the shutdown operation completes immediately.
        REQUIRE(mongoac_future_poll(future));

        auto const t3 = std::chrono::steady_clock::now();
        CHECK_NOTHROW(mongoac_runtime_make_progress(runtime));
        auto const t4 = std::chrono::steady_clock::now();
        auto const d2 = t4 - t3;

        // After a task is complete, it is dequeued from the scheduler.
        CHECK(d2 <= d1);
    }
}

TEST_CASE("make_progress_for", "[mongoac][runtime]") {
    auto const client_owner =
        REQUIRE_MAKE_UNIQUE(mongoac_client_new(doesnotexist_uri(), nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
    auto const runtime = runtime_owner.get();

    SECTION("null") {
        CHECK_NOTHROW(mongoac_runtime_make_progress_for(nullptr, 0u));
    }

    SECTION("basic") {
        static constexpr auto now = [] { return std::chrono::steady_clock::now(); };
        static constexpr auto to_ms = [](auto duration) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        };

        auto const duration = GENERATE(values<std::uint64_t>({0u, 1u, 2u, 5u, 10u, 25u, 50u, 100u}));

        auto const start = now();
        CHECK_NOTHROW(mongoac_runtime_make_progress_for(runtime, duration));
        auto const stop = now();

        REQUIRE(stop >= start);
        auto const dt = stop - start;
        CAPTURE(dt);
        CHECK(to_ms(dt) >= std::chrono::milliseconds(duration));
    }
}
