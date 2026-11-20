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

#include <mongoac/string.h>

#include <array>
#include <utility>

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

namespace mongoac::test {

namespace detail {

template <typename T, typename Arr, std::size_t... Idxs>
constexpr auto to_array(Arr&& arr, std::index_sequence<Idxs...>) {
    return std::array<std::remove_cv_t<T>, sizeof...(Idxs)>{{std::forward<Arr>(arr)[Idxs]...}};
}

} // namespace detail

template <typename T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&arr)[N]) {
    static_assert(!std::is_array_v<T>);
    static_assert(std::is_constructible_v<std::remove_cv_t<T>, T&>);

    return detail::to_array<T>(arr, std::make_index_sequence<N>{});
}

template <typename T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&&arr)[N]) {
    static_assert(!std::is_array_v<T>);
    static_assert(std::is_constructible_v<std::remove_cv_t<T>, T>);

    return detail::to_array<T>(std::move(arr), std::make_index_sequence<N>{});
}

} // namespace mongoac::test

#endif // !MONGOAC_TEST_TO_ARRAY_CXX20()

namespace mongoac::test {

template <typename ArrayT, void (*deleter)(ArrayT)>
class unique_array_ptr {
   private:
    ArrayT _value = {}; // mongoac_*_array_t

   public:
    ~unique_array_ptr() {
        deleter(_value);
    }

    unique_array_ptr(unique_array_ptr&& other) noexcept : _value(std::exchange(other._value, ArrayT{})) {}

    unique_array_ptr& operator=(unique_array_ptr&& other) noexcept {
        if (this != &other) {
            auto tmp = std::move(other);
            this->swap(tmp);
        }
        return *this;
    }

    unique_array_ptr(unique_array_ptr const& other) noexcept = delete;
    unique_array_ptr& operator=(unique_array_ptr const& other) noexcept = delete;

    unique_array_ptr() = default;

    explicit unique_array_ptr(ArrayT value) : _value(value) {}

    ArrayT value() const {
        return _value;
    }

    auto begin() const {
        return _value.ptr;
    }

    auto end() const {
        return _value.ptr ? _value.ptr + _value.len : nullptr;
    }

    void swap(unique_array_ptr& other) noexcept {
        std::swap(_value, other._value);
    }

    void reset() noexcept {
        *this = {};
    }
};

inline auto make_unique(mongoac_string_view_array_t array) {
    return unique_array_ptr<mongoac_string_view_array_t, &mongoac_string_view_array_destroy>{array};
}

} // namespace mongoac::test
