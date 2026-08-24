// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

/*
 * Dispatcher only: the compiler-specific macro definitions live in one
 * self-contained header per compiler (CompilerMSVC.h, CompilerClang.h,
 * CompilerGCC.h).  All three define the same set of macros, so the
 * files can be compared side by side with a diff tool.
 */

#if defined(_MSC_VER) && !defined(__clang__)
#  include "CompilerMSVC.h"
#elif defined(__clang__)
#  include "CompilerClang.h"
#elif defined(__GNUC__)
#  include "CompilerGCC.h"
#else
#  error Untested compiler.  Please add a Compiler<name>.h for it.
#endif
