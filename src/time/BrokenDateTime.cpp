// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "BrokenDateTime.hpp"
#include "Calendar.hxx"
#include "Convert.hxx"

#ifdef _WIN32
# include "FileTime.hxx"
#endif

#include <cassert>

#include <time.h>

#ifndef HAVE_POSIX
# include <sysinfoapi.h>
# include <timezoneapi.h>
#endif

static const BrokenDateTime ToBrokenDateTime(const struct tm &tm) noexcept {
  BrokenDateTime dt;

  dt.year = tm.tm_year + 1900;
  dt.month = tm.tm_mon + 1;
  dt.day = tm.tm_mday;
  dt.day_of_week = tm.tm_wday;

  dt.hour = tm.tm_hour;
  dt.minute = tm.tm_min;
  dt.second = tm.tm_sec;

  return dt;
}

BrokenDateTime::BrokenDateTime(std::chrono::system_clock::time_point tp) noexcept
  :BrokenDateTime(FromUnixTimeUTC(std::chrono::system_clock::to_time_t(tp))) {}

std::chrono::system_clock::time_point
BrokenDateTime::ToTimePoint() const noexcept
{
  assert(IsPlausible());

  struct tm tm;
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = second;
  tm.tm_isdst = 0;
  #ifdef _WIN32
    return TimeGm(tm) - std::chrono::hours(1);  // why ??
  #else
    return TimeGm(tm);
  #endif
}

const BrokenDateTime
BrokenDateTime::NowUTC() noexcept
{
  return BrokenDateTime{std::chrono::system_clock::now()};
}

const BrokenDateTime
BrokenDateTime::NowLocal() noexcept
{
  return ToBrokenDateTime(LocalTime(std::chrono::system_clock::now()));
}

BrokenDateTime
#ifdef HAVE_POSIX // ??
BrokenDateTime::FromUnixTimeUTC(int64_t t) noexcept
#else
BrokenDateTime::FromUnixTimeUTC(const time_t &t) noexcept
#endif
{
  return ToBrokenDateTime(GmTime(std::chrono::system_clock::from_time_t(t)));
}
