// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#if __cplusplus > 201703L

#include <bit>

namespace fizzy
{
template <typename To, class From>
using bit_cast = std::bit_cast<To, From>;
}

#else

#include <type_traits>

namespace fizzy
{
/// The non-constexpr implementation of C++20's std::bit_cast.
/// See https://en.cppreference.com/w/cpp/numeric/bit_cast.
template <class To, class From>
[[nodiscard]] inline
    typename std::enable_if_t<sizeof(To) == sizeof(From) && std::is_trivially_copyable_v<From> &&
                                  std::is_trivially_copyable_v<To>,
        To>
    bit_cast(const From& src) noexcept
{
    static_assert(std::is_trivially_constructible_v<To>);  // Additional, non-standard requirement.

    To dst;
    __builtin_memcpy(&dst, &src, sizeof(To));
    return dst;
}
}  // namespace fizzy

#endif /* __cplusplus > 201703L */
