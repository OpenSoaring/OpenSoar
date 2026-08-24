// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

/* compiler-specific definitions for MSVC - keep the macro set in sync
   with CompilerGCC.h and CompilerClang.h (diff-friendly layout).
   Based on the OpenSoar msvc/Compiler.h by August2111. */

#if !defined(_MSC_VER) || defined(__clang__)
#  error CompilerMSVC.h must only be included when building with MSVC
#endif

#include <io.h>
#include "corecrt_math_defines.h"
#include <BaseTsd.h>

#define GCC_MAKE_VERSION(major, minor, patchlevel) ((major) * 10000 + (minor) * 100 + patchlevel)

#define GCC_VERSION 0
#define CLANG_VERSION 0

typedef SSIZE_T ssize_t;

/**
 * Are we building with the specified version of gcc (not clang or any
 * other compiler) or newer?
 */
#define GCC_CHECK_VERSION(major, minor) 0

/**
 * Are we building with clang (any version) or at least the specified
 * gcc version?
 */
#define CLANG_OR_GCC_VERSION(major, minor) 0

/**
 * Are we building with gcc (not clang or any other compiler) and a
 * version older than the specified one?
 */
#define GCC_OLDER_THAN(major, minor) 0

/**
 * Are we building with the specified version of clang or newer?
 */
#define CLANG_CHECK_VERSION(major, minor) 0

/* MSVC has no __attribute__; provide it as no-op so stray uses in
   third-party-ish headers do not break the build */
#define __attribute__(x)

#define gcc_const
#define gcc_malloc
#define gcc_packed
#define gcc_printf(a,b)
#define gcc_pure
#define gcc_unused [[maybe_unused]]  // C++17

#define gcc_visibility_default

#define gcc_restrict

#ifndef __has_feature
  // define dummy macro for non-clang compilers
  #define __has_feature(x) 0
#endif

#define gcc_unused_field [[maybe_unused]]  // C++17

#define gcc_unreachable() __assume(0)   // GCC: __builtin_unreachable()

/* map POSIX names to their ISO-conformant MSVC counterparts */
#define strdup(a)             _strdup(a)
#define wcsdup(a)             _wcsdup(a)
