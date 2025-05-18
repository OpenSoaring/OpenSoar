// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

// use in Windows the same (valid!) path separator like all others:
#define DIR_SEPARATOR '/'
#define DIR_SEPARATOR_S "/"

static inline bool
IsDirSeparator(char ch)
{
#ifdef _WIN32
  // at Windows both separators are possible!!!
  return ch == DIR_SEPARATOR || ch == '\\';
#else
  return ch == DIR_SEPARATOR;
#endif
}
