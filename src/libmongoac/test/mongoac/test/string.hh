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

#include <string>
#include <string_view>

#include <mongoac/string.h>

namespace mongoac::test {

inline std::string_view from_mongoac(mongoac_string_view_t v) {
    if (v.ptr == nullptr) {
        return {};
    }

    return std::string_view{v.ptr, v.len};
}

inline std::string from_mongoac(mongoac_string_t const& v) {
    std::string ret;

    // Copy-only.
    if (v.ptr) {
        ret.assign(v.ptr, v.len);
    }

    return ret;
}

inline std::string from_mongoac(mongoac_string_t&& v) {
    std::string ret;

    // Copy then destroy.
    if (v.ptr) {
        ret.assign(v.ptr, v.len);
        mongoac_string_destroy(v);
    }

    return ret;
}

} // namespace mongoac::test
