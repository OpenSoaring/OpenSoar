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

  [[maybe_unused]]
  static std::string
  ms_time_str(std::chrono::system_clock::time_point time = std::chrono::system_clock::now(),
      std::string_view fmt_str = "%Y%m%d_%H%M%S") {
    char buffer[80];

    auto transformed = time.time_since_epoch().count() / 1000000;

    int millis = transformed % 1000;

    std::time_t tt = std::chrono::system_clock::to_time_t(time);
    auto size = strftime(buffer, sizeof(buffer), fmt_str.data(), localtime(&tt));
    snprintf(buffer+size, sizeof(buffer)-size, ".%03d", millis);

    return std::string(buffer);
  }

  static inline time_t
    TimeRaster(time_t t, time_t raster, uint16_t offset = 0) {
    return (((t - 1) / raster) + offset) * raster;
  }

  // static_assert(std::is_trivial<DateTime>::value, "type is not trivial");

};