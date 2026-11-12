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

#include <array>

#if defined(__cpp_lib_to_array) && __cpp_lib_to_array >= 201907L
#define MONGOAC_TEST_TO_ARRAY_CXX20() true
#else
#define MONGOAC_TEST_TO_ARRAY_CXX20() false
#endif

#if MONGOAC_TEST_TO_ARRAY_CXX20()

namespace mongoac::test {

using std::to_array;

} // namespace mongoac::test

#else // MONGOAC_TEST_TO_ARRAY_CXX20() ^|v !MONGOAC_TEST_TO_ARRAY_CXX20()

#include <cstddef>
#include <type_traits>
#include <utility>

namespace mongoac::test {

namespace detail {

template <typename T, std::size_t N, typename Arr, std::size_t... Idxs>
constexpr auto to_array(Arr&& arr, std::index_sequence<Idxs...>) {
    return std::array<std::remove_cv_t<T>, N>{{std::forward<Arr>(arr)[Idxs]...}};
}

} // namespace detail

template <typename T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&arr)[N]) {
    static_assert(!std::is_array_v<T>);
    static_assert(std::is_constructible_v<std::remove_cv_t<T>, T&>);

    return detail::to_array<T, N>(arr, std::make_index_sequence<N>{});
}

template <typename T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&&arr)[N]) {
    static_assert(!std::is_array_v<T>);
    static_assert(std::is_constructible_v<std::remove_cv_t<T>, T&>);

    return detail::to_array<T, N>(std::move(arr), std::make_index_sequence<N>{});
}

} // namespace mongoac::test

#endif // !MONGOAC_TEST_TO_ARRAY_CXX20()
