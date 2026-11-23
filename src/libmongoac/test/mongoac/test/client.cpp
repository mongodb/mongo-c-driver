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

#include <mongoac/test/client.hh>

//

#include <mongoac/test/string.hh>

#include <mongoac/string.h>

#include <cstdlib>
#include <string_view>

using mongoac::test::to_mongoac;

namespace mongoac::test {

mongoac_string_view_t default_uri() {
    if (auto const env = std::getenv("MONGOAC_TEST_URI")) {
        return to_mongoac(std::string_view(env));
    }

    return to_mongoac("mongodb://localhost:27017");
}

} // namespace mongoac::test
