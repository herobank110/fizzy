// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifdef __cpp_lib_bitops

#include <bit>

namespace fizzy
{
template <class T>
constexpr int popcount(T x) noexcept
{
    static_assert(sizeof(x) == sizeof(unsigned int) || sizeof(x) == sizeof(unsigned long long));
    return std::popcount(x);
}
}  // namespace fizzy

#else

#include <cstdint>

namespace fizzy
{
constexpr int popcount(uint32_t x) noexcept
{
    static_assert(sizeof(x) == sizeof(unsigned int));
    return __builtin_popcount(x);
}

constexpr int popcount(uint64_t x) noexcept
{
    static_assert(sizeof(x) == sizeof(unsigned long long));
    return __builtin_popcountll(x);
}
}  // namespace fizzy

#endif /* __cpp_lib_bitops */
