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

#include <mongoac/test/memory.hh>
#include <mongoac/test/string.hh>

#include <mongoac/client.h>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <chrono>
#include <cstdint>

using mongoac::test::make_unique;
using mongoac::test::operator""_sv;

namespace {

constexpr auto default_uri = "mongodb://doesnotexist.invalid?serverSelectionTimeoutMS=1000"_sv;

} // namespace

TEST_CASE("destroy", "[mongoac][runtime]") {
    SECTION("null") {
        CHECK_NOTHROW(mongoac_runtime_destroy(nullptr));
    }

    SECTION("client") {
        auto client = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri, nullptr), &mongoac_client_destroy);
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
    auto const client_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri, nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();

    SECTION("null") {
        auto const copy = make_unique(mongoac_runtime_clone(nullptr), &mongoac_runtime_destroy);
        CHECK(copy == nullptr);
    }

    SECTION("default") {
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

    SECTION("default") {
        auto const client = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri, nullptr), &mongoac_client_destroy);
        auto const runtime = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client.get()), &mongoac_runtime_destroy);

        CHECK(mongoac_runtime_address(runtime.get()) != 0u);
    }
}

TEST_CASE("make_progress", "[mongoac][runtime]") {
    auto const client_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri, nullptr), &mongoac_client_destroy);
    auto const client = client_owner.get();
    auto const runtime_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_get_runtime(client), &mongoac_runtime_destroy);
    auto const runtime = runtime_owner.get();

    SECTION("null") {
        CHECK_NOTHROW(mongoac_runtime_make_progress(nullptr));
    }

    SECTION("basic") {
        CHECK_NOTHROW(mongoac_runtime_make_progress(runtime));
    }
}

TEST_CASE("make_progress_for", "[mongoac][runtime]") {
    auto const client_owner = REQUIRE_MAKE_UNIQUE(mongoac_client_new(default_uri, nullptr), &mongoac_client_destroy);
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
