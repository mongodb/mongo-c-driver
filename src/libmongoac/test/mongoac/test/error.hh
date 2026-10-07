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

#pragma once

// Capture the error message and code before asserting `error` is equal to the given error category.
#define CHECK_MONGOAC_ERROR_CATEGORY(error, category)                                   \
    if (1) {                                                                            \
        auto const msg = ::mongoac::test::from_mongoac(::mongoac_error_message(error)); \
        CAPTURE(msg);                                                                   \
        CAPTURE(::mongoac_error_code(error));                                           \
        CHECK(::mongoac_error_category(error) == (category));                           \
    } else                                                                              \
        ((void)0)

// Capture the error message and category before asserting `error` is OK (regardless of error category).
#define CHECK_MONGOAC_ERROR_OK(error)                                                   \
    if (1) {                                                                            \
        auto const msg = ::mongoac::test::from_mongoac(::mongoac_error_message(error)); \
        CAPTURE(msg);                                                                   \
        CAPTURE(::mongoac_error_category(error));                                       \
        CHECK(::mongoac_error_code(error) == MONGOAC_ERROR_CODE_OK);                    \
    } else                                                                              \
        ((void)0)

// Capture the error message and code before asserting `error` is equal to the given mongoac error code.
#define CHECK_MONGOAC_ERROR_CODE(error, code)                                           \
    if (1) {                                                                            \
        auto const msg = ::mongoac::test::from_mongoac(::mongoac_error_message(error)); \
        CAPTURE(msg);                                                                   \
        if (::mongoac_error_category(error) == MONGOAC_ERROR_CATEGORY_MONGOAC) {        \
            CHECK(::mongoac_error_code(error) == (code));                               \
        } else {                                                                        \
            CAPTURE(::mongoac_error_code(error));                                       \
            CHECK(::mongoac_error_category(error) == MONGOAC_ERROR_CATEGORY_MONGOAC);   \
        }                                                                               \
    } else                                                                              \
        ((void)0)

// Capture the error message before asserting `error` is a mongoac invalid argument error.
#define CHECK_MONGOAC_ERROR_INVALID_ARGUMENT(error) CHECK_MONGOAC_ERROR_CODE(error, MONGOAC_ERROR_CODE_INVALID_ARGUMENT)

// Capture the error message before asserting `error` is a mongoac runtime error.
#define CHECK_MONGOAC_ERROR_RUNTIME_ERROR(error) CHECK_MONGOAC_ERROR_CODE(error, MONGOAC_ERROR_CODE_RUNTIME_ERROR)

// Assert that `error` contains the given substring in its error message.
#define CHECK_MONGOAC_ERROR_MESSAGE_CONTAINS(error, str) \
    CHECK_THAT(::mongoac::test::from_mongoac(::mongoac_error_message(error)), Catch::Matchers::ContainsSubstring(str))
