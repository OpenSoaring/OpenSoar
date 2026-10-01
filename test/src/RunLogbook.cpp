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

    if (line.starts_with("HFPLT"))
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
  const std::string igc_file;

public:
  std::vector<LogbookEntry> flights;

  FileHandler(const Waypoints &_waypoints, IgcHeader _header,
              std::string _igc_file) noexcept
    :waypoints(_waypoints), header(std::move(_header)),
     igc_file(std::move(_igc_file)) {}

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

  std::string GetLogbookIgcFile() noexcept override {
    return igc_file;
  }

  void OnLogbookFlight(const LogbookEntry &entry) noexcept override {
    flights.push_back(entry);
  }
};

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
        const Options &options)
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

  FileHandler handler(waypoints,
                      igc ? ReadIgcHeader(path) : IgcHeader{},
                      igc ? path.filename().string() : std::string{});
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
  }

  /* IGC files often end within a minute of the landing, before the
     program (which keeps running) would have confirmed it; such a
     flight is completed at the last fix, marked in the remark */
  const bool cut_short = recorder.IsInFlight();
  if (cut_short) {
    DerivedInfo &calculated = replay->SetCalculated();
    logbook.Solve(options.handicap, true, calculated.logbook_stats);
    recorder.FinishAtEnd(replay->Basic(), calculated,
                         "landing not confirmed, end of file");
  }

  for (const auto &flight : handler.flights)
    printf("%s\n", Logbook::FormatLine(flight).c_str());

  fprintf(stderr, "%s\t%zu flight(s)\t%u fixes%s\n",
          path.string().c_str(), handler.flights.size(), fixes,
          cut_short ? "\tlast one ends with the file" : "");

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

  printf("%s\n", Logbook::GetHeader());

  unsigned total = 0;
  for (const auto &path : files)
    total += RunFile(path, waypoints, options);

  fprintf(stderr, "%zu file(s), %u flight(s)\n", files.size(), total);
  return EXIT_SUCCESS;
} catch (...) {
  PrintException(std::current_exception());
  return EXIT_FAILURE;
}
