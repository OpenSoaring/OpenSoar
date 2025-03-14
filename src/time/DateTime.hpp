// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#ifdef __MSVC__
// with fmt version 11.1.4 there is an runtime error ;-(
// # define USE_STD_FORMAT  // only with fmt version: 10.2.1
#endif

# ifdef USE_STD_FORMAT
#   include <format>
    using std::string_view_literals::operator""sv;
# else
#   include <iomanip>  // put_time
#   include <sstream>  // stringstream
// #   include <fmt/format.h>
# endif

#include <ctime>
#include <chrono>
#include <string>
#include <string_view>

namespace DateTime {
  [[maybe_unused]]
  static time_t
    now() {
    return std::time(0);
  }

  [[maybe_unused]]
  static std::string
  time_str(time_t time, const std::string_view fmt_str = "%Y%m%d_%H%M%S")
  {
# ifdef USE_STD_FORMAT
    std::string fmt = std::string("{:");
    fmt += fmt_str;
    fmt += "}";
    fmt = std::vformat(fmt, std::make_format_args(time));
    return fmt;
# else
    std::stringstream s;
    s << std::put_time(std::gmtime(&time), fmt_str.data());
    return s.str();
# endif
  }

  [[maybe_unused]]
  static std::string
  str_now(std::string_view fmt_str = "%Y%m%d_%H%M%S") {
# ifdef USE_STD_FORMAT
    auto now = floor<std::chrono::seconds>(std::chrono::system_clock::now());
    std::string fmt = std::string("{:");
    fmt += fmt_str;
    fmt += "}";
    return std::vformat(fmt, std::make_format_args(now));
# else
    std::stringstream s;
    std::time_t now = DateTime::now(); // get time now
    s << std::put_time(std::gmtime(&now), fmt_str.data());
    return s.str();
# endif
  }

  // static_assert(std::is_trivial<DateTime>::value, "type is not trivial");

};