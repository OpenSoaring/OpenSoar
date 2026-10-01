// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "time/BrokenDateTime.hpp"
#include "Engine/Contest/LogbookStatistics.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class Path;
class BufferedOutputStream;

/**
 * One flight of the log book (file logbook.csv).
 *
 * The file is a table with one line per flight, separated by
 * semicolons, so a spreadsheet opens it directly and the pilot can
 * keep or print it beside the paper log book.  Times are UTC, as in
 * the IGC file; distances are kilometres.
 */
struct LogbookEntry {
  enum class Launch : uint8_t {
    UNKNOWN,
    WINCH,
    AEROTOW,
    SELF,
  };

  BrokenDateTime takeoff = BrokenDateTime::Invalid();
  BrokenDateTime landing = BrokenDateTime::Invalid();

  std::string takeoff_place, landing_place;

  Launch launch = Launch::UNKNOWN;

  std::string pilot, copilot;

  std::string aircraft, registration, competition_id;

  /** Free distance (OLC classic, six legs) in metres; 0 if none */
  double free_distance = 0;

  /** DMSt distance in metres and points; 0 if none */
  double dmst_distance = 0, dmst_points = 0;

  LogbookStatistics::DMStShape dmst_shape =
    LogbookStatistics::DMStShape::NONE;

  /** Maximum altitude in metres, negative if unknown */
  int max_altitude = -1;

  /** Name of the IGC file (without directory), empty if none */
  std::string igc_file;

  std::string remark;

  [[gnu::pure]]
  bool HasFlightTime() const noexcept {
    return takeoff.IsPlausible() && landing.IsPlausible() &&
      !(landing < takeoff);
  }

  [[gnu::pure]]
  std::chrono::system_clock::duration GetFlightTime() const noexcept {
    return HasFlightTime()
      ? landing - takeoff
      : std::chrono::system_clock::duration{};
  }
};

namespace Logbook {

/**
 * The token of a launch type in the file; empty for UNKNOWN.
 */
[[gnu::const]]
const char *
ToString(LogbookEntry::Launch launch) noexcept;

[[gnu::pure]]
LogbookEntry::Launch
ParseLaunch(std::string_view s) noexcept;

[[gnu::const]]
const char *
ToString(LogbookStatistics::DMStShape shape) noexcept;

[[gnu::pure]]
LogbookStatistics::DMStShape
ParseDMStShape(std::string_view s) noexcept;

/**
 * Split one line of the file into its columns.  A column may be
 * quoted ("..."), then it may contain semicolons and doubled quotes.
 */
std::vector<std::string>
SplitLine(std::string_view line) noexcept;

/**
 * Parse one line; returns false for the header and for lines that
 * have no plausible takeoff time.
 */
bool
ParseLine(std::string_view line, LogbookEntry &entry) noexcept;

/**
 * Format one entry as a line of the file, without the line end.
 */
std::string
FormatLine(const LogbookEntry &entry) noexcept;

/**
 * The first line of the file: the names of the columns.
 */
[[gnu::const]]
const char *
GetHeader() noexcept;

/**
 * Read all entries of the file, oldest first.  A missing file is
 * an empty log book.
 *
 * Throws on I/O error.
 */
std::vector<LogbookEntry>
Read(Path path);

/**
 * Append one entry; writes the header first if the file is new.
 *
 * Throws on I/O error.
 */
void
Append(Path path, const LogbookEntry &entry);

/**
 * Replace the file with these entries (after the pilot edited one).
 *
 * Throws on I/O error.
 */
void
Write(Path path, const std::vector<LogbookEntry> &entries);

} // namespace Logbook
