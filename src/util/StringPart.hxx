// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <string_view>

/**
 * Does the string carry the part as a whole, bounded by one of the
 * separator characters or the ends of the string?  "CB2-CH70" is a
 * part of "OV-3.2.20.1-CB2-CH70.img.gz", but "CH7" is not: a match
 * inside a longer part does not count.  An empty part matches always.
 */
[[gnu::pure]]
constexpr bool
StringHasPart(std::string_view s, std::string_view part,
              std::string_view separators = "-.") noexcept
{
  if (part.empty())
    return true;

  for (std::size_t i = s.find(part); i != s.npos; i = s.find(part, i + 1)) {
    const bool starts = i == 0 || separators.find(s[i - 1]) != s.npos;
    const std::size_t end = i + part.size();
    const bool ends = end == s.size() || separators.find(s[end]) != s.npos;
    if (starts && ends)
      return true;
  }

  return false;
}
