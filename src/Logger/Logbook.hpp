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

  /**
   * When the scored flight began: the release from the tow or winch,
   * or the end of the engine run of a self-launch; invalid if not
   * seen
   */
  BrokenDateTime release = BrokenDateTime::Invalid();

  std::string takeoff_place, landing_place;

  Launch launch = Launch::UNKNOWN;

  std::string pilot, copilot;

  std::string aircraft, registration, competition_id;

  /** Free distance (OLC classic, six legs) in metres; 0 if none */
  double free_distance = 0;

  /**
   * The speed on the free distance in km/h: the distance divided by
   * the time from its first to its last point; 0 if none
   */
  double free_speed = 0;

  /** DMSt distance in metres and points; 0 if none */
  double dmst_distance = 0, dmst_points = 0;

  LogbookStatistics::DMStShape dmst_shape =
    LogbookStatistics::DMStShape::NONE;

  /** Maximum altitude in metres, negative if unknown */
  int max_altitude = -1;

  /** Name of the flight log file (without directory), empty if none */
  std::string log_file;

  /**
   * The kind of #log_file in capitals ("IGC", "NMEA"), for filtering
   * in a spreadsheet
   */
  std::string file_type;

  /**
   * The flight recorder that wrote #log_file: the three letter code
   * of its manufacturer from the A record of the IGC file (e.g. "LXV",
   * "FLA", "XCS" for this program), and the type from the header
   * record HFFTYFRTYPE (e.g. "LXNAVIGATION,LX9000"); empty if
   * unknown, e.g. for an NMEA log
   */
  std::string recorder_code, recorder_type;

  /**
   * The serial number of the flight recorder from the A record of the
   * IGC file ("ALXVJNI" is code "LXV", serial "JNI"); with the code,
   * it identifies the logger (see LoggerSettings::recorders)
   */
  std::string recorder_serial;

  /**
   * The files of the other recordings of this flight (another logger,
   * the NMEA log, a restart), best first, separated by
   * Logbook::OTHER_FILES_SEPARATOR; #log_file is the best one
   */
  std::string other_files;

  /** The id of the flight on WeGlide after the upload, 0 if none */
  uint64_t weglide_id = 0;

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
 * The letter of a launch type in the file: W, E, F or U (unknown).
 */
[[gnu::const]]
const char *
ToString(LogbookEntry::Launch launch) noexcept;

/**
 * Parse a launch type: the letters ToString() writes, S and A as their
 * English counterparts, and the words of the first version.
 */
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
 * Which column of the file holds which value.  The default is the
 * order this version writes; ParseHeader() takes it from the header,
 * so files of older versions and files rearranged in a spreadsheet are
 * read as well.
 */
struct ColumnMap {
  static constexpr unsigned MAX_VALUES = 32;

  /** column of each value of #LogbookEntry, -1 if missing */
  int index[MAX_VALUES];

  ColumnMap() noexcept;

  /**
   * @return false if this is not a header line (map unchanged)
   */
  bool ParseHeader(std::string_view line) noexcept;
};

/**
 * Parse one line; returns false for the header and for lines that
 * have no plausible takeoff time.
 */
bool
ParseLine(std::string_view line, LogbookEntry &entry,
          const ColumnMap &map = {}) noexcept;

/**
 * Quote a text column for the file if it needs it.
 */
std::string
Quote(std::string_view s) noexcept;

/**
 * The kind of a flight log file for #LogbookEntry::file_type: its
 * extension in capitals ("IGC", "NMEA"), empty without one.
 */
[[gnu::pure]]
std::string
FileTypeOf(std::string_view filename) noexcept;

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

/**
 * Separates the names in LogbookEntry::other_files.  A comma is rare in
 * the name of a log file, and a spreadsheet keeps the column together
 * because the log book separates its columns with semicolons.
 */
constexpr std::string_view OTHER_FILES_SEPARATOR = ", ";

/**
 * Is the file one of the other recordings of the entry?
 */
[[gnu::pure]]
bool
ContainsFile(const LogbookEntry &entry, std::string_view name) noexcept;

/**
 * Note the WeGlide flight id at the entry recorded by this log file
 * (as #log_file or among the other files), after the upload.
 *
 * @return false if no entry has this file
 *
 * Throws on I/O error.
 */
bool
SetWeGlideFlightId(Path path, std::string_view log_file, uint64_t id);

} // namespace Logbook
