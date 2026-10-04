// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Logbook.hpp"
#include "Geo/GeoPoint.hpp"
#include "Geo/CoordinateFormat.hpp"
#include "system/Path.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class Waypoints;
struct LoggerSettings;
class OperationEnvironment;
struct DeviceRegister;

/**
 * Builds the log book from recorded files (IGC files and NMEA logs),
 * and keeps it at one entry per flight.
 *
 * A flight is often recorded more than once: by this program's IGC
 * logger, by one or two external loggers whose files are downloaded
 * later, and in the NMEA log.  Each recording is read with the same
 * code that records the flight live (LogbookReplay, TraceComputer,
 * LogbookComputer, LogbookRecorder); the recordings of one flight are
 * then joined into one entry, taken from the best recording, with
 * empty fields filled from the others.
 */
namespace Logbook {

/**
 * The pilot, the glider and the recorder from the header of an IGC
 * file.
 */
struct IgcHeader {
  std::string pilot, copilot, aircraft, registration, competition_id;

  /**
   * The manufacturer code and the serial number from the A record
   * ("ALXVJNI" is code "LXV", serial "JNI"); they identify the logger
   */
  std::string recorder_code, recorder_serial;

  /** The recorder type from HFFTYFRTYPE */
  std::string recorder_type;
};

/**
 * Read the header of an IGC file.  An unreadable file gives an empty
 * header.
 */
IgcHeader
ReadIgcHeader(Path path) noexcept;

using Clock = std::chrono::system_clock;

/**
 * The position once a minute, to tell whether two files recorded the
 * same aircraft.
 */
struct TrackSample {
  Clock::time_point minute;
  GeoPoint location;
};

/**
 * One flight as one file recorded it.
 */
struct Recording {
  LogbookEntry entry;

  /** The file (UTF-8), for messages */
  std::string source;

  /** The positions of this flight, by time */
  std::vector<TrackSample> track;

  Clock::time_point Begin() const noexcept {
    return entry.takeoff.ToTimePoint();
  }

  Clock::time_point End() const noexcept {
    return entry.landing.IsPlausible()
      ? entry.landing.ToTimePoint()
      : Begin();
  }

  Clock::duration Duration() const noexcept {
    return End() - Begin();
  }
};

/**
 * The flight recorders named in the settings (Recorder 1 and 2), best
 * first, each as in the A record of its IGC files ("LXVJNI").
 */
using RecorderList = std::vector<std::string>;

/**
 * Bring a recorder as the pilot typed it into the form of the A
 * record: capitals, without spaces ("lxv jni" becomes "LXVJNI").
 */
[[gnu::pure]]
std::string
NormalizeRecorder(std::string_view s) noexcept;

/**
 * The recorders named in the logger settings.
 */
RecorderList
GetRecorders(const LoggerSettings &settings) noexcept;

struct ReadSettings {
  /** The waypoints that name the places, nullptr for coordinates */
  const Waypoints *waypoints = nullptr;

  /** The handicap of the glider for the DMSt points */
  unsigned handicap = 100;

  /** How places without a waypoint are written */
  CoordinateFormat coordinate_format = CoordinateFormat::DDMM_MMM;

  /** The driver for the device sentences of NMEA logs, nullptr for
      the generic NMEA parser */
  const DeviceRegister *driver = nullptr;

  /** The recorders whose files are preferred, see GetRank() */
  RecorderList recorders;
};

struct FileResult {
  std::vector<Recording> flights;

  /** The number of fixes read */
  unsigned fixes = 0;

  /** The last flight was still going at the end of the file */
  bool cut_short = false;

  /** Reading was cancelled; #flights is incomplete */
  bool cancelled = false;
};

/**
 * Read all flights of one file.
 *
 * @param env checked for cancellation while reading; may be nullptr
 *
 * Throws on error (e.g. the file cannot be opened).
 */
FileResult
ReadFile(Path path, const ReadSettings &settings,
         OperationEnvironment *env = nullptr);

/**
 * Does the entry describe a whole flight, from the takeoff to the
 * landing?  A recording that began or ended in flight does not.
 */
[[gnu::pure]]
bool
IsComplete(const LogbookEntry &entry) noexcept;

/**
 * How much a recording is preferred over the others of the same flight
 * of (nearly) the same length; higher is better.  The files of the
 * recorders named in the settings come first, Recorder 1 before
 * Recorder 2, then this program's own IGC file, then the files of
 * other loggers, an IGC file before an NMEA log.
 */
[[gnu::pure]]
unsigned
GetRank(const LogbookEntry &entry, const RecorderList &recorders) noexcept;

/**
 * One flight with the recordings joined.
 */
struct Flight {
  /** The entry, from the best recording */
  Recording kept;

  /** The files of the other recordings of this flight */
  std::vector<std::string> others;
};

/**
 * Join the recordings of each flight; two recordings are of the same
 * flight if the aircraft was at the same place at the same time.
 *
 * @return the flights, by takeoff
 */
std::vector<Flight>
JoinRecordings(std::vector<Recording> recordings,
               const RecorderList &recorders) noexcept;

/**
 * Add an entry to the log book, or let it replace the entry of the
 * same flight if it is the better one.  Without positions in the log
 * book, an entry of the same flight is one whose flight time overlaps
 * by at least half of the shorter one; all files of one data directory
 * are the pilot's own flights.  An entry that is replaced keeps the
 * crew and the aircraft, which the pilot may have corrected.
 *
 * @return true if the log book was changed
 */
bool
MergeEntry(std::vector<LogbookEntry> &entries,
           const LogbookEntry &entry,
           const RecorderList &recorders) noexcept;

/**
 * Merge entries into the log book file (created if it does not exist)
 * and write it sorted by takeoff.  The file is read again just before,
 * so an entry another part of the program has written in the meantime
 * is not lost.
 *
 * Throws on error.
 */
void
MergeIntoFile(Path path, const std::vector<LogbookEntry> &entries,
              const RecorderList &recorders);

/**
 * A recorded file that is not in the log book yet.
 */
struct NewFile {
  AllocatedPath path;

  /** The key in the index: name and size */
  std::string key;
};

/**
 * Find the IGC files and NMEA logs in the given folders which are not
 * listed in the index file.  The index lists every file read for the
 * log book with its size, including the files without a flight, so
 * they are not read at every start; a file that has grown since (a
 * recording continued after a restart) is read again.
 *
 * @return the files, by name
 */
std::vector<NewFile>
FindNewFiles(Path index_path, const std::vector<AllocatedPath> &folders);

/**
 * Note in the index that these files have been read.
 *
 * Throws on error.
 */
void
AddToIndex(Path index_path, const std::vector<std::string> &keys);

struct UpdateResult {
  unsigned files_read = 0, flights = 0;
  bool cancelled = false;
};

/**
 * Read the files, join their flights, merge them into the log book
 * and note the files in the index.  When cancelled, the flights of the
 * files read so far are kept; the other files are read the next time.
 *
 * Throws on error.
 */
UpdateResult
Update(Path logbook_path, Path index_path,
       const std::vector<NewFile> &files,
       const ReadSettings &settings, OperationEnvironment &env);

/**
 * Before rebuilding: rename the log book to logbook-YYYY-MM-DD.csv,
 * after its last flight (a number is added if that exists), and
 * delete the index, so all files are read again.
 *
 * @return the new name of the old log book, nullptr if there was none
 *
 * Throws on error.
 */
AllocatedPath
Retire(Path logbook_path, Path index_path);

} // namespace Logbook
