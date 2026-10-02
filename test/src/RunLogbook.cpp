// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

/*
 * Run IGC and NMEA files through the log book and print the entries
 * as logbook.csv would hold them.
 *
 * The flight state, the traces, the free and DMSt distance and the
 * recording of the entry are the code the program runs
 * (FlyingComputer, TraceComputer, LogbookComputer, LogbookRecorder),
 * so a pilot can check the log book against years of his own flights
 * without replaying each of them on the device.
 *
 * Usage: RunLogbook [--waypoints=FILE]... [--handicap=N]
 *                   [--driver=NAME] FILE_OR_DIRECTORY...
 *
 * Directories are searched recursively for *.igc and *.nmea files.
 * The table goes to standard output, a line per file with the number
 * of flights (and what was odd about it) to standard error.
 *
 * The table has two columns more than logbook.csv: the file a flight
 * comes from, and, for a flight that another file recorded as well
 * (a second logger, or the NMEA log of the same flight), the log file
 * of the entry that is kept.  Filtering out the rows with something
 * in "Duplicate of" leaves one row per flight.
 */

#include "DebugReplayIGC.hpp"
#include "DebugReplayNMEA.hpp"
#include "Computer/TraceComputer.hpp"
#include "Computer/LogbookComputer.hpp"
#include "Computer/Settings.hpp"
#include "Logger/Logbook.hpp"
#include "Logger/LogbookPlace.hpp"
#include "Logger/LogbookRecorder.hpp"
#include "Engine/Waypoint/Waypoints.hpp"
#include "Waypoint/WaypointReader.hpp"
#include "Waypoint/Factory.hpp"
#include "Geo/CoordinateFormat.hpp"
#include "NMEA/MoreData.hpp"
#include "NMEA/Derived.hpp"
#include "Operation/Operation.hpp"
#include "system/Path.hpp"
#include "util/PrintException.hxx"
#include "util/StringCompare.hxx"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/**
 * The pilot and the glider from the header of an IGC file, so the
 * entries can be compared with the flights in the file.
 */
struct IgcHeader {
  std::string pilot, copilot, aircraft, registration, competition_id;

  /** manufacturer code from the A record, and the recorder type */
  std::string recorder_code, recorder_type;
};

static std::string
HeaderValue(const std::string &line)
{
  /* "HFGTYGLIDERTYPE:ASW 28" and "HFGTY Glider type :ASW 28" */
  const auto colon = line.find(':');
  std::string value = colon == line.npos
    ? line.substr(5)
    : line.substr(colon + 1);

  while (!value.empty() && (value.back() == ' ' || value.back() == '\r'))
    value.pop_back();
  while (!value.empty() && value.front() == ' ')
    value.erase(0, 1);
  return value;
}

static IgcHeader
ReadIgcHeader(const fs::path &path)
{
  IgcHeader header;
  std::ifstream file(path);
  std::string line;

  while (std::getline(file, line)) {
    if (line.starts_with("B"))
      /* the header ends where the fixes begin */
      break;

    if (line.starts_with("A") && line.size() >= 4)
      /* "ALXV..." - the first three letters name the manufacturer */
      header.recorder_code = line.substr(1, 3);
    else if (line.starts_with("HFFTY"))
      header.recorder_type = HeaderValue(line);
    else if (line.starts_with("HFPLT"))
      header.pilot = HeaderValue(line);
    else if (line.starts_with("HFCM2"))
      header.copilot = HeaderValue(line);
    else if (line.starts_with("HFGTY"))
      header.aircraft = HeaderValue(line);
    else if (line.starts_with("HFGID"))
      header.registration = HeaderValue(line);
    else if (line.starts_with("HFCID"))
      header.competition_id = HeaderValue(line);
  }

  return header;
}

class FileHandler final : public LogbookRecorder::Handler {
  const Waypoints &waypoints;
  const IgcHeader header;
  const std::string log_file;

public:
  std::vector<LogbookEntry> flights;

  FileHandler(const Waypoints &_waypoints, IgcHeader _header,
              std::string _log_file) noexcept
    :waypoints(_waypoints), header(std::move(_header)),
     log_file(std::move(_log_file)) {}

  void OnLogbookTakeoff(LogbookEntry &entry) noexcept override {
    entry.pilot = header.pilot;
    entry.copilot = header.copilot;
    entry.aircraft = header.aircraft;
    entry.registration = header.registration;
    entry.competition_id = header.competition_id;
  }

  std::string FindLogbookPlace(const GeoPoint &location) noexcept override {
    return Logbook::FindPlace(&waypoints, location,
                              CoordinateFormat::DDMM_MMM);
  }

  std::string GetLogbookFile() noexcept override {
    return log_file;
  }

  void FillLogbookRecorder(LogbookEntry &entry) noexcept override {
    entry.recorder_code = header.recorder_code;
    entry.recorder_type = header.recorder_type;
  }

  void OnLogbookFlight(const LogbookEntry &entry) noexcept override {
    flights.push_back(entry);
  }
};

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
 * One flight of the table, with where it comes from.
 */
struct Row {
  LogbookEntry entry;
  std::string source;

  /** The positions of this flight, by time */
  std::vector<TrackSample> track;

  /** The log file of the entry kept of this flight, if this is not it */
  std::string duplicate_of;

  std::chrono::system_clock::time_point Begin() const noexcept {
    return entry.takeoff.ToTimePoint();
  }

  std::chrono::system_clock::time_point End() const noexcept {
    return entry.landing.IsPlausible()
      ? entry.landing.ToTimePoint()
      : Begin();
  }

  std::chrono::system_clock::duration Duration() const noexcept {
    return End() - Begin();
  }

  /** The program confirmed the landing and saw the takeoff */
  bool IsComplete() const noexcept {
    return entry.remark.empty();
  }
};

/**
 * Do two rows describe the same flight?  Two loggers in one glider
 * are at the same place at the same time; a recording that began in
 * flight (a restart) lies within the other one.  Comparing the times
 * alone would join the flights of a club day, so the positions are
 * compared minute by minute: nearly all common minutes must be within
 * 1 km.  A tug and its glider are that close only during the tow.
 */
[[gnu::pure]]
static bool
IsSameFlight(const Row &a, const Row &b) noexcept
{
  if (a.End() < b.Begin() || b.End() < a.Begin())
    return false;

  unsigned common = 0, near = 0;
  auto i = a.track.begin(), j = b.track.begin();
  while (i != a.track.end() && j != b.track.end()) {
    if (i->minute < j->minute)
      ++i;
    else if (j->minute < i->minute)
      ++j;
    else {
      ++common;
      if (i->location.DistanceS(j->location) < 1000)
        ++near;
      ++i;
      ++j;
    }
  }

  return common >= 5 && near * 10 >= common * 9;
}

/**
 * Which of two rows of the same flight should be kept?  The one
 * covering (nearly) the whole flight, then one with a confirmed
 * landing and a seen takeoff, then an IGC file (signed, with the
 * header of the glider) before an NMEA log, then the longer one.
 */
[[gnu::pure]]
static bool
IsBetter(const Row &a, const Row &b,
         std::chrono::system_clock::duration longest) noexcept
{
  const bool a_full = a.Duration() * 10 >= longest * 9;
  const bool b_full = b.Duration() * 10 >= longest * 9;
  if (a_full != b_full)
    return a_full;

  if (a.IsComplete() != b.IsComplete())
    return a.IsComplete();

  const bool a_igc = a.entry.file_type == "IGC";
  const bool b_igc = b.entry.file_type == "IGC";
  if (a_igc != b_igc)
    return a_igc;

  if (a.Duration() != b.Duration())
    return a.Duration() > b.Duration();

  return a.source < b.source;
}

/**
 * Group the rows into flights and mark all but the best row of each.
 * The few hundred flights of a pilot allow comparing every pair.
 *
 * @return the number of flights
 */
static unsigned
MarkDuplicates(std::vector<Row> &rows) noexcept
{
  /* each row points to a row of its flight, the first of a flight to
     itself */
  std::vector<std::size_t> group(rows.size());
  for (std::size_t i = 0; i < rows.size(); ++i)
    group[i] = i;

  const auto find = [&](std::size_t i){
    while (group[i] != i)
      i = group[i];
    return i;
  };

  for (std::size_t i = 0; i < rows.size(); ++i)
    for (std::size_t j = i + 1; j < rows.size(); ++j)
      if (IsSameFlight(rows[i], rows[j]))
        group[find(j)] = find(i);

  unsigned flights = 0;
  for (std::size_t g = 0; g < rows.size(); ++g) {
    if (find(g) != g)
      continue;

    ++flights;

    std::chrono::system_clock::duration longest{};
    for (std::size_t i = 0; i < rows.size(); ++i)
      if (find(i) == g)
        longest = std::max(longest, rows[i].Duration());

    std::size_t best = g;
    for (std::size_t i = 0; i < rows.size(); ++i)
      if (find(i) == g && IsBetter(rows[i], rows[best], longest))
        best = i;

    for (std::size_t i = 0; i < rows.size(); ++i)
      if (find(i) == g && i != best)
        rows[i].duplicate_of = rows[best].entry.log_file;
  }

  return flights;
}

struct Options {
  unsigned handicap = 100;
  std::string driver = "Generic";
};

static bool
IsIgc(const fs::path &path)
{
  return StringEndsWithIgnoreCase(path.string().c_str(), ".igc");
}

static bool
IsNmea(const fs::path &path)
{
  return StringEndsWithIgnoreCase(path.string().c_str(), ".nmea");
}

/**
 * Replay one file as the calculation thread and the log book of the
 * program would see it.
 */
static unsigned
RunFile(const fs::path &path, const Waypoints &waypoints,
        const Options &options, std::vector<Row> &rows)
{
  const bool igc = IsIgc(path);
  std::unique_ptr<DebugReplay> replay{
    igc
    ? DebugReplayIGC::Create(Path(path.string().c_str()))
    : DebugReplayNMEA::Create(Path(path.string().c_str()), options.driver)};
  if (!replay) {
    fprintf(stderr, "%s\tcannot be read\n", path.string().c_str());
    return 0;
  }

  std::vector<TrackSample> track;

  FileHandler handler(waypoints,
                      igc ? ReadIgcHeader(path) : IgcHeader{},
                      path.filename().string());
  LogbookRecorder recorder(handler);

  /* only the switches TraceComputer asks for */
  ComputerSettings settings{};
  settings.contest.enable = false;
  settings.logger.enable_flight_logger = true;

  TraceComputer trace;
  LogbookComputer logbook(trace.GetFull(), trace.GetContest());

  bool last_flying = false;
  unsigned fixes = 0;

  while (replay->Next()) {
    const MoreData &basic = replay->Basic();
    DerivedInfo &calculated = replay->SetCalculated();
    ++fixes;

    /* the order of GlideComputer: the trace first, then the reset at
       the takeoff (GlideComputer::OnTakeoff) */
    trace.Update(settings, basic, calculated);

    if (calculated.flight.flying && !last_flying) {
      trace.Reset();
      logbook.Reset();
      calculated.logbook_stats.Reset();
    }
    last_flying = calculated.flight.flying;

    logbook.Process(calculated.flight, options.handicap, false,
                    calculated.logbook_stats);
    recorder.Update(basic, calculated);

    if (basic.location_available && basic.time_available &&
        basic.date_time_utc.IsPlausible()) {
      const auto minute =
        std::chrono::floor<std::chrono::minutes>(basic.date_time_utc.ToTimePoint());
      if (track.empty() || track.back().minute != minute)
        track.push_back({minute, basic.location});
    }
  }

  /* IGC files often end within a minute of the landing, before the
     program (which keeps running) would have confirmed it; such a
     flight is completed at the last fix, marked in the remark */
  const bool cut_short = recorder.IsInFlight();
  if (cut_short) {
    DerivedInfo &calculated = replay->SetCalculated();
    logbook.SolveFinal(options.handicap, calculated.logbook_stats);
    recorder.FinishAtEnd(replay->Basic(), calculated,
                         "landing not confirmed, end of file");
  }

  for (const auto &flight : handler.flights) {
    Row row{flight, path.string(), {}, {}};
    for (const auto &sample : track)
      if (sample.minute >= row.Begin() && sample.minute <= row.End())
        row.track.push_back(sample);
    rows.push_back(std::move(row));
  }

  fprintf(stderr, "%s\t%zu flight(s)\t%u fixes%s%s\n",
          path.string().c_str(), handler.flights.size(), fixes,
          cut_short ? "\tlast one ends with the file" : "",
          !handler.flights.empty() && handler.flights.front().remark.starts_with("recording began")
          ? "\tbegins in flight" : "");

  return handler.flights.size();
}

static void
CollectFiles(const fs::path &path, std::vector<fs::path> &files)
{
  if (fs::is_directory(path)) {
    for (const auto &i : fs::recursive_directory_iterator(path))
      if (i.is_regular_file() && (IsIgc(i.path()) || IsNmea(i.path())))
        files.push_back(i.path());
  } else
    files.push_back(path);
}

int
main(int argc, char **argv)
try {
  Options options;
  Waypoints waypoints;
  std::vector<fs::path> files;
  /* quiet: standard output is the table */
  NullOperationEnvironment operation;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};
    if (arg.starts_with("--waypoints=")) {
      /* waypoints from a file name places; temporary ones do not
         (see Logbook::FindPlace()) */
      ReadWaypointFile(Path(argv[i] + 12), waypoints,
                       WaypointFactory(WaypointOrigin::PRIMARY),
                       operation);
    } else if (arg.starts_with("--handicap=")) {
      options.handicap = std::strtoul(argv[i] + 11, nullptr, 10);
    } else if (arg.starts_with("--driver=")) {
      options.driver = std::string{arg.substr(9)};
    } else if (arg.starts_with("-")) {
      fprintf(stderr,
              "Usage: %s [--waypoints=FILE]... [--handicap=N] "
              "[--driver=NAME] FILE_OR_DIRECTORY...\n", argv[0]);
      return EXIT_FAILURE;
    } else
      CollectFiles(argv[i], files);
  }

  waypoints.Optimise();
  std::sort(files.begin(), files.end());

  std::vector<Row> rows;
  for (const auto &path : files)
    RunFile(path, waypoints, options, rows);

  std::stable_sort(rows.begin(), rows.end(),
                   [](const Row &a, const Row &b){
                     return a.Begin() < b.Begin();
                   });
  const unsigned flights = MarkDuplicates(rows);

  printf("%s;Duplicate of;Source file\n", Logbook::GetHeader());
  for (const auto &row : rows)
    printf("%s;%s;%s\n", Logbook::FormatLine(row.entry).c_str(),
           Logbook::Quote(row.duplicate_of).c_str(),
           Logbook::Quote(row.source).c_str());

  fprintf(stderr, "%zu file(s), %zu entries, %u flight(s)\n",
          files.size(), rows.size(), flights);
  return EXIT_SUCCESS;
} catch (...) {
  PrintException(std::current_exception());
  return EXIT_FAILURE;
}
