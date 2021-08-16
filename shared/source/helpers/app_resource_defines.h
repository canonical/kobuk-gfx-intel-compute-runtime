/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <type_traits>

namespace NEO {
namespace AppResourceDefines {
#if defined(_DEBUG) || (_RELEASE_INTERNAL)
constexpr bool resourceTagSupport = true;
#else
constexpr bool resourceTagSupport = false;
#endif

template <typename TMemberSource, typename = int>
static constexpr bool has_ResourceTag = false;

template <typename TMemberSource>
static constexpr bool has_ResourceTag<TMemberSource, decltype((void)TMemberSource::ResourceTag, int{})> = true;

constexpr uint32_t maxStrLen = 8u;
} // namespace AppResourceDefines
} // namespace NEO
