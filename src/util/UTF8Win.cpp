// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "UTF8Win.hpp"
#include "LogFile.hpp"

#ifdef _WIN32
#include <stringapiset.h>

std::wstring 
UTF8ToWide(const std::string_view s)
{
  int length = MultiByteToWideChar(CP_UTF8, 0, s.data(), s.size() + 1, nullptr, 0);
  std::wstring w(length + 1, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), s.size() + 1, w.data(), length);
  return w;
}

#endif
