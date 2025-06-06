// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "BrokenDate.hpp"
#include "BrokenTime.hpp"

#include <chrono>
#include <type_traits>

/**
 * A broken-down representation of date and time.
 */
struct BrokenDateTime : public BrokenDate, public BrokenTime {
  /**
   * Non-initializing default constructor.
   */
  BrokenDateTime() noexcept = default;

  constexpr
  BrokenDateTime(unsigned _year, unsigned _month, unsigned _day,
                 unsigned _hour, unsigned _minute, unsigned _second=0) noexcept
    :BrokenDate(_year, _month, _day), BrokenTime(_hour, _minute, _second) {}

  constexpr
  BrokenDateTime(unsigned _year, unsigned _month, unsigned _day) noexcept
    :BrokenDate(_year, _month, _day), BrokenTime(0, 0) {}

  constexpr
  BrokenDateTime(const BrokenDate &date, const BrokenTime &time) noexcept
    :BrokenDate(date), BrokenTime(time) {}

  explicit BrokenDateTime(std::chrono::system_clock::time_point tp) noexcept;

  constexpr const BrokenDate &GetDate() const noexcept {
    return *this;
  }

  constexpr const BrokenTime &GetTime() const noexcept {
    return *this;
  }

  constexpr
  bool operator==(const BrokenDateTime other) const noexcept {
    return GetDate() == other.GetDate() && GetTime() == other.GetTime();
  }

  constexpr bool operator>(const BrokenDateTime other) const noexcept {
    return GetDate() > other.GetDate() ||
      (GetDate() == other.GetDate() && GetTime() > other.GetTime());
  }

  constexpr bool operator<(const BrokenDateTime other) const noexcept {
    return other > *this;
  }

  /**
   * Returns an instance that fails the Plausible() check.
   */
  constexpr
  static BrokenDateTime Invalid() noexcept {
    return BrokenDateTime(BrokenDate::Invalid(), BrokenTime::Invalid());
  }

  /**
   * Does the "date" part of this object contain plausible values?
   */
  constexpr
  bool IsDatePlausible() const noexcept {
    return BrokenDate::IsPlausible();
  }

  /**
   * Does the "time" part of this object contain plausible values?
   */
  constexpr
  bool IsTimePlausible() const noexcept {
    return BrokenTime::IsPlausible();
  }

  /**
   * Does this object contain plausible values?
   */
  constexpr
  bool IsPlausible() const noexcept {
    return IsDatePlausible() && IsTimePlausible();
  }

  /**
   * Returns a new #BrokenDateTime with the same date at midnight.
   */
  constexpr
  BrokenDateTime AtMidnight() const noexcept {
    return BrokenDateTime(*this, BrokenTime::Midnight());
  }

  /**
   * Convert a UNIX UTC time stamp (seconds since epoch) to a
   * BrokenDateTime object.
   */
  [[gnu::const]]
  static BrokenDateTime FromUnixTime(const int64_t t) noexcept;

  [[gnu::pure]]
  std::chrono::system_clock::time_point ToTimePoint() const noexcept;

  /**
   * Returns the current system date and time, in UTC.
   */
  [[gnu::pure]]
  static const BrokenDateTime NowUTC() noexcept;

  /**
   * Returns the current system date and time, in the current time zone.
   */
  [[gnu::pure]]
  static const BrokenDateTime NowLocal() noexcept;

  /**
   * Returns a BrokenDateTime that has the specified number of seconds
   * added to it.
   *
   * @param seconds the number of seconds to add; may be negative
   */
  [[gnu::pure]]
  BrokenDateTime operator+(std::chrono::system_clock::duration delta) const noexcept {
    return BrokenDateTime{ToTimePoint() + delta};
  }

  [[gnu::pure]]
  BrokenDateTime operator-(std::chrono::system_clock::duration delta) const noexcept {
    return BrokenDateTime{ToTimePoint() - delta};
  }

  /**
   * Returns the number of seconds between the two BrokenDateTime structs.
   * The second one is subtracted from the first one.
   *
   * <now> - <old> = positive timespan since <old>
   */
  [[gnu::pure]]
  std::chrono::system_clock::duration operator-(const BrokenDateTime &other) const noexcept {
    return ToTimePoint() - other.ToTimePoint();
  }
};

static_assert(std::is_trivial<BrokenDateTime>::value, "type is not trivial");

