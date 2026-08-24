// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

/* compiler-specific definitions for GCC - keep the macro set in sync
   with CompilerClang.h and CompilerMSVC.h (diff-friendly layout) */

#if !defined(__GNUC__) || defined(__clang__)
#  error CompilerGCC.h must only be included when building with GCC
#endif

#define GCC_MAKE_VERSION(major, minor, patchlevel) ((major) * 10000 + (minor) * 100 + patchlevel)

#define GCC_VERSION GCC_MAKE_VERSION(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#define CLANG_VERSION 0

/**
 * Are we building with the specified version of gcc (not clang or any
 * other compiler) or newer?
 */
#define GCC_CHECK_VERSION(major, minor) \
  (GCC_VERSION >= GCC_MAKE_VERSION(major, minor, 0))

/**
 * Are we building with clang (any version) or at least the specified
 * gcc version?
 */
#define CLANG_OR_GCC_VERSION(major, minor) \
  (GCC_CHECK_VERSION(major, minor))

/**
 * Are we building with gcc (not clang or any other compiler) and a
 * version older than the specified one?
 */
#define GCC_OLDER_THAN(major, minor) \
  (GCC_VERSION < GCC_MAKE_VERSION(major, minor, 0))

#if GCC_OLDER_THAN(10,0)
#  error Sorry, your gcc version is too old.  You need at least version 10.
#endif

/**
 * Are we building with the specified version of clang or newer?
 */
#define CLANG_CHECK_VERSION(major, minor) 0

#define gcc_const __attribute__((const))
#define gcc_malloc __attribute__((malloc))
#define gcc_packed __attribute__((packed))
#define gcc_printf(a,b) __attribute__((format(printf, a, b)))
#define gcc_pure __attribute__((pure))
#define gcc_unused __attribute__((unused))

#define gcc_visibility_default __attribute__((visibility("default")))

#ifndef __cplusplus
/* plain C99 has "restrict" */
#define gcc_restrict restrict
#else
/* "__restrict__" is a GCC extension for C++ */
#define gcc_restrict __restrict__
#endif

#ifndef __has_feature
  // define dummy macro for non-clang compilers
  #define __has_feature(x) 0
#endif

#if __has_feature(attribute_unused_on_fields)
#define gcc_unused_field gcc_unused
#else
#define gcc_unused_field
#endif

#define gcc_unreachable() __builtin_unreachable()
