// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <stdio.h>

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#endif  // defined(__GNUC__)

template<typename... Args>
static inline int
StringFormat(char *buffer, size_t size, const char *const fmt,
	     Args&&... args) noexcept
{
  return snprintf(buffer, size, fmt, args...);
}

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif  // defined(__GNUC__)

template<typename... Args>
static inline int
StringFormatUnsafe(char *buffer, const char *fmt, Args&&... args) noexcept
{
  return sprintf(buffer, fmt, args...);
}

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif  // defined(__GNUC__)
