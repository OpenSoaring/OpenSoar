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
 * Usage: RunLogbook [--datapath=DIR] [--waypoints=FILE]... [--handicap=N]
 *                   [--driver=NAME] [--recorder=ID]... [--move-no-flight]
 *                   [--move-igc] [FILE_OR_DIRECTORY...]
 *
 * --recorder names Recorder 1 and 2 of the logger settings, as in the A
 * record of their IGC files (e.g. --recorder=LXVJNI); their files give
 * the entry of a flight first.
 *
 * Directories are searched recursively for *.igc and *.nmea files,
 * except in folders named "no-flight".  With --move-no-flight, a file
 * that was read without finding a flight in it (a recording on the
 * ground) is moved into a folder "no-flight" beside it, so the next
 * run skips it; nothing is deleted, and a file can simply be moved
 * back.
 *
 * The program writes its IGC files (and those downloaded from a
 * logger) into the folder "igc" of the data directory, the NMEA logs
 * into "logs"; older versions wrote both into "logs".  Both folders
 * are always searched.  With --move-igc, an IGC file in a folder
 * "logs" is first moved into the folder "igc" beside it.
 * Without files, the data directory of the program is searched (most
 * flights are in its "logs" folder): the one given by --datapath, or
 * the one the program itself would use (OpenSoarData).  Without
 * --waypoints, the waypoint files at the top of that data directory
 * name the places.
 *
 * The table goes to standard output, a line per file with the number
 * of flights (and what was odd about it) to standard error.
 *
 * The table has one row per flight, as the log book of the program:
 * the recordings of a flight (several loggers, the NMEA log of the
 * same flight, a restart) are joined, and the best one gives the
 * entry.  Two columns follow those of logbook.csv: the other files
 * of the flight, and the file the entry comes from.
 */

#include "Logger/Logbook.hpp"
#include "Logger/LogbookBuilder.hpp"
#include "Device/Register.hpp"
#include "LocalPath.hpp"
#include "Engine/Waypoint/Waypoints.hpp"
#include "Waypoint/WaypointReader.hpp"
#include "Waypoint/Factory.hpp"
#include "Operation/Operation.hpp"
#include "system/Path.hpp"
#include "util/PrintException.hxx"
#include "util/StringCompare.hxx"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/**
 * The UTF-8 name of a file, as #Path expects it on all platforms
 * (path::string() would use the ANSI code page on Windows).
 */
static std::string
ToUtf8(const fs::path &path)
{
  const auto u8 = path.u8string();
  return {reinterpret_cast<const char *>(u8.data()), u8.size()};
}

static fs::path
FromUtf8(const char *s)
{
  return fs::path{std::u8string{reinterpret_cast<const char8_t *>(s)}};
}

struct Options {
  unsigned handicap = 100;
  std::string driver = "Generic";

  /** move files without a flight into "no-flight" */
  bool move_no_flight = false;

  /** move IGC files from "logs" into "igc" */
  bool move_igc = false;
};

/**
 * The folder for files without a flight; directories of this name are
 * not searched.
 */
static constexpr const char *NO_FLIGHT_DIR = "no-flight";

/**
 * Move a file into the folder @p dir (created if needed).  An existing
 * file of the same name there is not replaced.
 *
 * @return the new path, or the old one if the file was not moved
 */
static fs::path
MoveIntoFolder(const fs::path &path, const fs::path &dir)
{
  const fs::path dest = dir / path.filename();

  std::error_code error;
  fs::create_directories(dir, error);
  if (!error && fs::exists(dest, error))
    error = std::make_error_code(std::errc::file_exists);
  if (!error)
    fs::rename(path, dest, error);

  if (error) {
    fprintf(stderr, "%s\tnot moved to %s: %s\n", ToUtf8(path).c_str(),
            ToUtf8(dir).c_str(), error.message().c_str());
    return path;
  }

  fprintf(stderr, "%s\tmoved to %s\n", ToUtf8(path).c_str(),
          ToUtf8(dir).c_str());
  return dest;
}

/**
 * Move a file without a flight into the folder "no-flight" beside it.
 */
static void
MoveToNoFlight(const fs::path &path)
{
  MoveIntoFolder(path, path.parent_path() / NO_FLIGHT_DIR);
}


static bool
IsIgc(const fs::path &path)
{
  return StringEndsWithIgnoreCase(ToUtf8(path).c_str(), ".igc");
}

static bool
IsNmea(const fs::path &path)
{
  return StringEndsWithIgnoreCase(ToUtf8(path).c_str(), ".nmea");
}

/**
 * The folders of the data directory: IGC files, and the NMEA logs (and
 * the IGC files of older versions).
 */
static constexpr const char *IGC_DIR = "igc", *LOGS_DIR = "logs";

/**
 * An IGC file in a folder "logs" goes into the folder "igc" beside it.
 *
 * @return the path of the file after that
 */
static fs::path
MoveIgcFromLogs(const fs::path &path)
{
  if (!IsIgc(path) || path.parent_path().filename() != LOGS_DIR)
    return path;

  return MoveIntoFolder(path, path.parent_path().parent_path() / IGC_DIR);
}

/**
 * Read one file as the calculation thread and the log book of the
 * program would see it (the same code the program uses to build its
 * log book).
 */
static unsigned
RunFile(const fs::path &path, const Logbook::ReadSettings &settings,
        const Options &options, std::vector<Logbook::Recording> &recordings)
{
  Logbook::FileResult result;
  try {
    result = Logbook::ReadFile(Path(ToUtf8(path).c_str()), settings);
  } catch (...) {
    fprintf(stderr, "%s\tcannot be read\n", ToUtf8(path).c_str());
    return 0;
  }

  const std::size_t n = result.flights.size();
  fprintf(stderr, "%s\t%zu flight(s)\t%u fixes%s%s\n",
          ToUtf8(path).c_str(), n, result.fixes,
          result.cut_short ? "\tlast one ends with the file" : "",
          n > 0 && result.flights.front().entry.remark.starts_with("recording began")
          ? "\tbegins in flight" : "");

  for (auto &flight : result.flights)
    recordings.push_back(std::move(flight));

  /* only a file that could be read to its end has really no flight;
     an unreadable one returned above.  The file is closed by now:
     Windows does not rename an open file. */
  if (n == 0 && options.move_no_flight)
    MoveToNoFlight(path);

  return n;
}

static bool
IsWaypointFile(const fs::path &path)
{
  const std::string name = ToUtf8(path);
  return StringEndsWithIgnoreCase(name.c_str(), ".cup") ||
    StringEndsWithIgnoreCase(name.c_str(), ".dat") ||
    StringEndsWithIgnoreCase(name.c_str(), ".wpt");
}

/**
 * Read the waypoint files at the top of the data directory, as a
 * replacement for the files the profile names.
 */
static void
ReadDataWaypoints(const fs::path &data_path, Waypoints &waypoints,
                  OperationEnvironment &operation)
{
  std::vector<fs::path> files;
  for (const auto &i : fs::directory_iterator(data_path))
    if (i.is_regular_file() && IsWaypointFile(i.path()))
      files.push_back(i.path());
  std::sort(files.begin(), files.end());

  for (const auto &file : files) {
    fprintf(stderr, "waypoints from %s\n", ToUtf8(file).c_str());
    try {
      ReadWaypointFile(Path(ToUtf8(file).c_str()), waypoints,
                       WaypointFactory(WaypointOrigin::PRIMARY), operation);
    } catch (...) {
      PrintException(std::current_exception());
    }
  }
}

static void
CollectFiles(const fs::path &path, std::vector<fs::path> &files)
{
  if (fs::is_directory(path)) {
    for (auto i = fs::recursive_directory_iterator(path,
                                                   fs::directory_options::skip_permission_denied);
         i != fs::recursive_directory_iterator(); ++i) {
      if (i->is_directory() && i->path().filename() == NO_FLIGHT_DIR)
        /* the files sorted out by an earlier --move-no-flight */
        i.disable_recursion_pending();
      else if (i->is_regular_file() &&
               (IsIgc(i->path()) || IsNmea(i->path())))
        files.push_back(i->path());
    }
  } else
    files.push_back(path);
}

int
main(int argc, char **argv)
try {
  Options options;
  Waypoints waypoints;
  bool have_waypoints = false;
  const char *data_path = nullptr;
  std::vector<fs::path> files;
  Logbook::RecorderList recorders;
  /* quiet: standard output is the table */
  NullOperationEnvironment operation;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};
    if (arg.starts_with("--waypoints=")) {
      /* waypoints from a file name places; temporary ones do not
         (see Logbook::FindPlace()) */
      ReadWaypointFile(Path(ToUtf8(argv[i] + 12).c_str()), waypoints,
                       WaypointFactory(WaypointOrigin::PRIMARY),
                       operation);
      have_waypoints = true;
    } else if (arg.starts_with("--datapath=")) {
      data_path = argv[i] + 11;
    } else if (arg.starts_with("--handicap=")) {
      options.handicap = std::strtoul(argv[i] + 11, nullptr, 10);
    } else if (arg.starts_with("--driver=")) {
      options.driver = std::string{arg.substr(9)};
    } else if (arg.starts_with("--recorder=")) {
      /* Recorder 1, then Recorder 2 of the logger settings */
      recorders.push_back(Logbook::NormalizeRecorder(arg.substr(11)));
    } else if (arg == "--move-no-flight") {
      options.move_no_flight = true;
    } else if (arg == "--move-igc") {
      options.move_igc = true;
    } else if (arg.starts_with("-")) {
      fprintf(stderr,
              "Usage: %s [--datapath=DIR] [--waypoints=FILE]... "
              "[--handicap=N] [--driver=NAME] [--recorder=ID]... "
              "[--move-no-flight] "
              "[--move-igc] [FILE_OR_DIRECTORY...]\n",
              argv[0]);
      return EXIT_FAILURE;
    } else
      CollectFiles(argv[i], files);
  }

  if (files.empty()) {
    /* the flights of the program's own data directory */
    fs::path data;
    if (data_path != nullptr)
      data = data_path;
    else {
      InitialiseDataPath();
      data = FromUtf8(GetPrimaryDataPath().c_str());
    }

    if (!fs::is_directory(data)) {
      fprintf(stderr, "No data directory %s; give one with --datapath= "
              "or name the files\n", ToUtf8(data).c_str());
      return EXIT_FAILURE;
    }

    fprintf(stderr, "data directory %s\n", ToUtf8(data).c_str());
    CollectFiles(data, files);
    if (!have_waypoints)
      ReadDataWaypoints(data, waypoints, operation);
  }

  waypoints.Optimise();
  std::sort(files.begin(), files.end());

  if (options.move_igc)
    for (auto &path : files)
      path = MoveIgcFromLogs(path);

  Logbook::ReadSettings settings;
  settings.waypoints = &waypoints;
  settings.handicap = options.handicap;
  settings.recorders = recorders;
  if (options.driver != "Generic") {
    settings.driver = FindDriverByName(options.driver.c_str());
    if (settings.driver == nullptr) {
      fprintf(stderr, "No such driver: %s\n", options.driver.c_str());
      return EXIT_FAILURE;
    }
  }

  std::vector<Logbook::Recording> recordings;
  for (const auto &path : files)
    RunFile(path, settings, options, recordings);

  const std::size_t entries = recordings.size();
  const auto flights = Logbook::JoinRecordings(std::move(recordings),
                                               settings.recorders);

  printf("%s;Also recorded in;Source file\n", Logbook::GetHeader());
  for (const auto &flight : flights) {
    std::string others;
    for (const auto &other : flight.others) {
      if (!others.empty())
        others += ", ";
      others += other;
    }

    printf("%s;%s;%s\n", Logbook::FormatLine(flight.kept.entry).c_str(),
           Logbook::Quote(others).c_str(),
           Logbook::Quote(flight.kept.source).c_str());
  }

  fprintf(stderr, "%zu file(s), %zu recording(s), %zu flight(s)\n",
          files.size(), entries, flights.size());
  return EXIT_SUCCESS;
} catch (...) {
  PrintException(std::current_exception());
  return EXIT_FAILURE;
}
