// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "UTF8.hpp"

#ifdef _WIN32
/* ATTENTION : codevct is dprecated with C++ 17 (and C++ 20) - what is
*              the replacement for? */
# include <codecvt> 
# include <string>
# include <locale>
#endif

#ifdef _WIN32

std::string 
ToUTF8(const std::string &str, const std::locale &loc)
{
  using wcvt = std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t>;
  std::u32string wstr(str.size(), U'\0');
  std::use_facet<std::ctype<char32_t>>(loc).widen(
      str.data(), str.data() + str.size(), &wstr[0]);
  return wcvt{}.to_bytes(wstr.data(), wstr.data() + wstr.size());
}

std::string 
FromUTF8(const std::string &str, const std::locale &loc)
{
  using wcvt = std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t>;
  auto wstr = wcvt{}.from_bytes(str);
  std::string result(wstr.size(), '0');
  std::use_facet<std::ctype<char32_t>>(loc).narrow(
      wstr.data(), wstr.data() + wstr.size(), '?', &result[0]);
  return result;
}
#endif