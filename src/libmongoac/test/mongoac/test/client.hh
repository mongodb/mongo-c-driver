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

#include <mongoac/test/string.hh>

#include <mongoac/string.h>

namespace mongoac::test {

// Return the value of env var `MONGOAC_TEST_URI` when set, otherwise "mongodb://localhost:27017".
mongoac_string_view_t default_uri();

// Return `mongodb://doesnotexist.invalid:27017?serverSelectionTimeoutMS=1`.
constexpr mongoac_string_view_t doesnotexist_uri() {
    // `serverSelectionTimeoutMS=1` is a defensive measure to ensure quick test failure when a test case actually does
    // attempt to connect to a cluster when it should not.
    return to_mongoac("mongodb://doesnotexist.invalid:27017?serverSelectionTimeoutMS=1");
}

} // namespace mongoac::test
