// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LogbookBuilder.hpp"
#include "LogbookPlace.hpp"
#include "LogbookRecorder.hpp"
#include "LogbookReplay.hpp"
#include "Computer/TraceComputer.hpp"
#include "Computer/LogbookComputer.hpp"
#include "Computer/Settings.hpp"
#include "Operation/Operation.hpp"
#include "io/FileLineReader.hpp"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "system/FileUtil.hpp"
#include "util/StringCompare.hxx"
#include "util/StringStrip.hxx"
#include "LogFile.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cstring>
#include <set>

namespace Logbook {

static std::string
HeaderValue(std::string_view line) noexcept
{
  /* "HFGTYGLIDERTYPE:ASW 28" and "HFGTY Glider type :ASW 28" */
  const auto colon = line.find(':');
  std::string_view value = colon == line.npos
    ? line.substr(std::min<std::size_t>(5, line.size()))
    : line.substr(colon + 1);

  while (!value.empty() && (value.back() == ' ' || value.back() == '\r'))
    value.remove_suffix(1);
  while (!value.empty() && value.front() == ' ')
    value.remove_prefix(1);

  /* what loggers write for a value nobody entered; filling another
     recording's field with it would hide the real value */
  for (const char *placeholder : {"undefined", "not set", "none", "unknown",
                                  "n/a", "-"})
    if (StringIsEqualIgnoreCase(value, placeholder))
      return {};

  return std::string{value};
}

IgcHeader
ReadIgcHeader(Path path) noexcept
try {
  IgcHeader header;
  FileLineReaderA reader(path);

  const char *line;
  while ((line = reader.ReadLine()) != nullptr) {
    const std::string_view s{line};
    if (s.starts_with("B"))
      /* the header ends where the fixes begin */
      break;

    if (s.starts_with("A") && s.size() >= 4) {
      /* "ALXVJNI..." - three letters name the manufacturer, the next
         three are the serial number of the logger */
      header.recorder_code = std::string{s.substr(1, 3)};
      std::string_view serial = s.substr(4, 3);
      while (!serial.empty() && (serial.back() == ' ' || serial.back() == '\r'))
        serial.remove_suffix(1);
      header.recorder_serial = std::string{serial};
    } else if (s.starts_with("HFFTY"))
      header.recorder_type = HeaderValue(s);
    else if (s.starts_with("HFPLT"))
      header.pilot = HeaderValue(s);
    else if (s.starts_with("HFCM2"))
      header.copilot = HeaderValue(s);
    else if (s.starts_with("HFGTY"))
      header.aircraft = HeaderValue(s);
    else if (s.starts_with("HFGID"))
      header.registration = HeaderValue(s);
    else if (s.starts_with("HFCID"))
      header.competition_id = HeaderValue(s);
  }

  return header;
} catch (...) {
  return {};
}

namespace {

/**
 * Collects the flights of one file; the file tells the crew and the
 * glider (IGC header), the waypoints name the places.
 */
class FileHandler final : public LogbookRecorder::Handler {
  const ReadSettings &settings;
  const IgcHeader &header;
  const std::string log_file;

public:
  std::vector<LogbookEntry> flights;

  FileHandler(const ReadSettings &_settings, const IgcHeader &_header,
              std::string _log_file) noexcept
    :settings(_settings), header(_header),
     log_file(std::move(_log_file)) {}

  void OnLogbookTakeoff(LogbookEntry &entry) noexcept override {
    entry.pilot = header.pilot;
    entry.copilot = header.copilot;
    entry.aircraft = header.aircraft;
    entry.registration = header.registration;
    entry.competition_id = header.competition_id;
  }

  std::string FindLogbookPlace(const GeoPoint &location) noexcept override {
    return FindPlace(settings.waypoints, location,
                     settings.coordinate_format);
  }

  std::optional<double>
  GetLogbookAirfieldElevation(const GeoPoint &location) noexcept override {
    return FindAirfieldElevation(settings.waypoints, location);
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

} // anonymous namespace

/**
 * How often reading a file checks for cancellation (in fixes): a long
 * NMEA log has a hundred thousand of them.
 */
static constexpr unsigned CANCEL_CHECK_INTERVAL = 4096;

FileResult
ReadFile(Path path, const ReadSettings &settings,
         OperationEnvironment *env)
{
  FileResult result;

  const bool igc = LogbookReplay::IsIgc(path);
  const IgcHeader header = igc ? ReadIgcHeader(path) : IgcHeader{};

  LogbookReplay replay(path, settings.driver);

  FileHandler handler(settings, header, path.GetBase().c_str());
  LogbookRecorder recorder(handler);

  /* only the switches TraceComputer asks for */
  ComputerSettings computer_settings{};
  computer_settings.contest.enable = false;
  computer_settings.logger.enable_flight_logger = true;

  TraceComputer trace;
  LogbookComputer logbook(trace.GetFull(), trace.GetContest());

  std::vector<TrackSample> track;
  bool last_flying = false;

  while (replay.Next()) {
    if (++result.fixes % CANCEL_CHECK_INTERVAL == 0 &&
        env != nullptr && env->IsCancelled()) {
      result.cancelled = true;
      return result;
    }

    const MoreData &basic = replay.Basic();
    DerivedInfo &calculated = replay.SetCalculated();

    /* the order of GlideComputer: the trace first, then the reset at
       the takeoff (GlideComputer::OnTakeoff) */
    trace.Update(computer_settings, basic, calculated);

    if (calculated.flight.flying && !last_flying) {
      trace.Reset();
      logbook.Reset();
      calculated.logbook_stats.Reset();
    }
    last_flying = calculated.flight.flying;

    logbook.Process(basic, calculated.flight, settings.handicap, false,
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
  result.cut_short = recorder.IsInFlight();
  if (result.cut_short) {
    DerivedInfo &calculated = replay.SetCalculated();
    logbook.SolveFinal(calculated.flight, settings.handicap,
                       calculated.logbook_stats);
    recorder.FinishAtEnd(replay.Basic(), calculated,
                         "landing not confirmed, end of file");
  }

  for (auto &entry : handler.flights) {
    Recording r{std::move(entry), path.c_str(), header.recorder_serial, {}};
    for (const auto &sample : track)
      if (sample.minute >= r.Begin() && sample.minute <= r.End())
        r.track.push_back(sample);
    result.flights.push_back(std::move(r));
  }

  return result;
}

bool
IsComplete(const LogbookEntry &entry) noexcept
{
  const std::string_view remark{entry.remark};
  /* "landing not confirmed, end of file" is no gap: a logger stops
     recording a minute after the landing, before the program would
     have confirmed it, and the recorder writes "ended in flight"
     instead if the glider was still fast */
  return remark.find("began in flight") == remark.npos &&
    remark.find("ended in flight") == remark.npos;
}

unsigned
GetRank(const LogbookEntry &entry,
        [[maybe_unused]] std::string_view serial) noexcept
{
  /* the loggers named in the settings (Recorder 1 and 2) will come
     first, identified by code and serial */
  if (entry.file_type == "IGC")
    return entry.recorder_code == "XCS" ? 2 : 1;

  return 0;
}

/**
 * Which of two recordings of the same flight should be kept?  The one
 * covering (nearly) the whole flight, then one with a confirmed
 * landing and a seen takeoff, then by rank (GetRank()), then the
 * longer one.
 */
[[gnu::pure]]
static bool
IsBetter(const LogbookEntry &a, std::string_view a_serial,
         const LogbookEntry &b, std::string_view b_serial,
         Clock::duration longest) noexcept
{
  const auto a_duration = a.GetFlightTime(), b_duration = b.GetFlightTime();

  const bool a_full = a_duration * 10 >= longest * 9;
  const bool b_full = b_duration * 10 >= longest * 9;
  if (a_full != b_full)
    return a_full;

  if (IsComplete(a) != IsComplete(b))
    return IsComplete(a);

  const unsigned a_rank = GetRank(a, a_serial), b_rank = GetRank(b, b_serial);
  if (a_rank != b_rank)
    return a_rank > b_rank;

  if (a_duration != b_duration)
    return a_duration > b_duration;

  return a.log_file < b.log_file;
}

/**
 * Fill the empty fields of @p dest from @p src, another recording of
 * the same flight: the IGC header of an external logger names the
 * glider, an engine sensor tells the launch.
 */
static void
FillEmpty(LogbookEntry &dest, const LogbookEntry &src) noexcept
{
  const auto fill = [](std::string &d, const std::string &s){
    if (d.empty())
      d = s;
  };

  fill(dest.pilot, src.pilot);
  fill(dest.copilot, src.copilot);
  fill(dest.aircraft, src.aircraft);
  fill(dest.registration, src.registration);
  fill(dest.competition_id, src.competition_id);

  if (dest.launch == LogbookEntry::Launch::UNKNOWN)
    dest.launch = src.launch;
  if (!dest.release.IsPlausible())
    dest.release = src.release;
  if (dest.max_altitude < 0)
    dest.max_altitude = src.max_altitude;
}

/**
 * Do two recordings describe the same flight?  Two loggers in one
 * glider are at the same place at the same time; a recording that
 * began in flight (a restart) lies within the other one.  Comparing
 * the times alone would join the flights of a club day, so the
 * positions are compared minute by minute: nearly all common minutes
 * must be within 1 km.  A tug and its glider are that close only
 * during the tow.
 */
[[gnu::pure]]
static bool
IsSameFlight(const Recording &a, const Recording &b) noexcept
{
  if (a.End() < b.Begin() || b.End() < a.Begin())
    return false;

  unsigned common = 0, close_by = 0;
  auto i = a.track.begin(), j = b.track.begin();
  while (i != a.track.end() && j != b.track.end()) {
    if (i->minute < j->minute)
      ++i;
    else if (j->minute < i->minute)
      ++j;
    else {
      ++common;
      if (i->location.DistanceS(j->location) < 1000)
        ++close_by;
      ++i;
      ++j;
    }
  }

  return common >= 5 && close_by * 10 >= common * 9;
}

std::vector<Flight>
JoinRecordings(std::vector<Recording> recordings) noexcept
{
  std::stable_sort(recordings.begin(), recordings.end(),
                   [](const Recording &a, const Recording &b){
                     return a.Begin() < b.Begin();
                   });

  /* each recording points to one of its flight, the first of a flight
     to itself; the few hundred flights of a pilot allow comparing
     every pair */
  const std::size_t n = recordings.size();
  std::vector<std::size_t> group(n);
  for (std::size_t i = 0; i < n; ++i)
    group[i] = i;

  const auto find = [&](std::size_t i){
    while (group[i] != i)
      i = group[i];
    return i;
  };

  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = i + 1; j < n; ++j)
      if (IsSameFlight(recordings[i], recordings[j]))
        group[find(j)] = find(i);

  std::vector<Flight> flights;
  for (std::size_t g = 0; g < n; ++g) {
    if (find(g) != g)
      continue;

    Clock::duration longest{};
    for (std::size_t i = 0; i < n; ++i)
      if (find(i) == g)
        longest = std::max(longest, recordings[i].entry.GetFlightTime());

    std::size_t best = g;
    for (std::size_t i = 0; i < n; ++i)
      if (find(i) == g &&
          IsBetter(recordings[i].entry, recordings[i].recorder_serial,
                   recordings[best].entry, recordings[best].recorder_serial,
                   longest))
        best = i;

    Flight flight{recordings[best], {}};
    for (std::size_t i = 0; i < n; ++i) {
      if (find(i) != g || i == best)
        continue;

      FillEmpty(flight.kept.entry, recordings[i].entry);
      flight.others.push_back(recordings[i].source);
    }

    flights.push_back(std::move(flight));
  }

  std::stable_sort(flights.begin(), flights.end(),
                   [](const Flight &a, const Flight &b){
                     return a.kept.Begin() < b.kept.Begin();
                   });
  return flights;
}

static Clock::time_point
BeginOf(const LogbookEntry &entry) noexcept
{
  return entry.takeoff.ToTimePoint();
}

static Clock::time_point
EndOf(const LogbookEntry &entry) noexcept
{
  return entry.HasFlightTime() ? entry.landing.ToTimePoint() : BeginOf(entry);
}

/**
 * Do two entries describe the same flight?  Their flight times overlap
 * by at least half of the shorter one, or the shorter one (without a
 * landing) begins within the other one.
 */
[[gnu::pure]]
static bool
IsSameFlight(const LogbookEntry &a, const LogbookEntry &b) noexcept
{
  if (!a.takeoff.IsPlausible() || !b.takeoff.IsPlausible())
    return false;

  const auto begin = std::max(BeginOf(a), BeginOf(b));
  const auto end = std::min(EndOf(a), EndOf(b));
  if (end < begin)
    return false;

  const auto shorter = std::min(EndOf(a) - BeginOf(a), EndOf(b) - BeginOf(b));
  return (end - begin) * 2 >= shorter;
}

bool
MergeEntry(std::vector<LogbookEntry> &entries,
           const LogbookEntry &entry) noexcept
{
  for (auto &old : entries) {
    if (!IsSameFlight(old, entry))
      continue;

    const auto longest = std::max(old.GetFlightTime(), entry.GetFlightTime());
    if (!IsBetter(entry, {}, old, {}, longest)) {
      FillEmpty(old, entry);
      return false;
    }

    LogbookEntry merged = entry;

    /* the crew and the aircraft of the entry in the log book stay: the
       pilot may have corrected them */
    const auto keep = [](std::string &d, const std::string &s){
      if (!s.empty())
        d = s;
    };
    keep(merged.pilot, old.pilot);
    keep(merged.copilot, old.copilot);
    keep(merged.aircraft, old.aircraft);
    keep(merged.registration, old.registration);
    keep(merged.competition_id, old.competition_id);

    FillEmpty(merged, old);
    old = std::move(merged);
    return true;
  }

  entries.push_back(entry);
  return true;
}

void
MergeIntoFile(Path path, const std::vector<LogbookEntry> &new_entries)
{
  auto entries = Read(path);

  bool changed = !File::Exists(path);
  for (const auto &entry : new_entries)
    changed |= MergeEntry(entries, entry);

  if (!changed)
    return;

  std::stable_sort(entries.begin(), entries.end(),
                   [](const LogbookEntry &a, const LogbookEntry &b){
                     return a.takeoff < b.takeoff;
                   });
  Write(path, entries);
}

static bool
IsRecordedFile(Path name) noexcept
{
  return StringEndsWithIgnoreCase(name.c_str(), ".igc") ||
    StringEndsWithIgnoreCase(name.c_str(), ".nmea");
}

static std::string
MakeKey(Path path) noexcept
{
  return fmt::format("{};{}", path.GetBase().c_str(), File::GetSize(path));
}

static std::set<std::string, std::less<>>
ReadIndex(Path index_path) noexcept
try {
  std::set<std::string, std::less<>> keys;
  if (!File::Exists(index_path))
    return keys;

  FileLineReaderA reader(index_path);
  char *line;
  while ((line = reader.ReadLine()) != nullptr) {
    StripRight(line);
    if (*line != 0)
      keys.emplace(line);
  }

  return keys;
} catch (...) {
  /* without the index, all files are read again; the log book keeps
     one entry per flight anyway */
  LogError(std::current_exception(), "Failed to read the log book index");
  return {};
}

std::vector<NewFile>
FindNewFiles(Path index_path, const std::vector<AllocatedPath> &folders)
{
  const auto known = ReadIndex(index_path);

  struct Visitor final : File::Visitor {
    const std::set<std::string, std::less<>> &known;
    std::vector<NewFile> files;

    explicit Visitor(const std::set<std::string, std::less<>> &_known) noexcept
      :known(_known) {}

    void Visit(Path path, Path filename) override {
      if (!IsRecordedFile(filename))
        return;

      std::string key = MakeKey(path);
      if (!known.contains(key))
        files.push_back({AllocatedPath{path}, std::move(key)});
    }
  } visitor{known};

  for (const auto &folder : folders)
    if (Directory::Exists(folder))
      Directory::VisitFiles(folder, visitor);

  std::sort(visitor.files.begin(), visitor.files.end(),
            [](const NewFile &a, const NewFile &b){
              return a.key < b.key;
            });
  return std::move(visitor.files);
}

void
AddToIndex(Path index_path, const std::vector<std::string> &keys)
{
  if (keys.empty())
    return;

  FileOutputStream file(index_path, FileOutputStream::Mode::APPEND_OR_CREATE);
  BufferedOutputStream writer(file);
  for (const auto &key : keys) {
    writer.Write(key);
    writer.Write("\r\n");
  }
  writer.Flush();
  file.Commit();
}

UpdateResult
Update(Path logbook_path, Path index_path,
       const std::vector<NewFile> &files,
       const ReadSettings &settings, OperationEnvironment &env)
{
  UpdateResult result;
  std::vector<Recording> recordings;
  std::vector<std::string> keys;

  env.SetProgressRange(files.size());

  for (std::size_t i = 0; i < files.size(); ++i) {
    if (env.IsCancelled()) {
      result.cancelled = true;
      break;
    }

    const auto &file = files[i];
    env.SetText(file.path.GetBase().c_str());
    env.SetProgressPosition(i);

    try {
      auto r = ReadFile(file.path, settings, &env);
      if (r.cancelled) {
        result.cancelled = true;
        break;
      }

      for (auto &flight : r.flights)
        recordings.push_back(std::move(flight));
    } catch (...) {
      /* a file that cannot be read now (e.g. locked) is not noted, so
         the next start tries again */
      LogError(std::current_exception(), "Log book: failed to read file");
      continue;
    }

    keys.push_back(file.key);
    ++result.files_read;
  }

  const auto flights = JoinRecordings(std::move(recordings));
  result.flights = flights.size();

  std::vector<LogbookEntry> entries;
  entries.reserve(flights.size());
  for (const auto &flight : flights)
    entries.push_back(flight.kept.entry);

  /* the log book first: if writing it fails, the files are read
     again next time */
  MergeIntoFile(logbook_path, entries);
  AddToIndex(index_path, keys);

  return result;
}

AllocatedPath
Retire(Path logbook_path, Path index_path)
{
  File::Delete(index_path);

  if (!File::Exists(logbook_path))
    return nullptr;

  /* named after the last flight, so several old log books are told
     apart; without a flight, after today */
  BrokenDate date = BrokenDate::Invalid();
  for (const auto &entry : Read(logbook_path))
    if (entry.takeoff.IsPlausible() &&
        (!date.IsPlausible() || date < (const BrokenDate &)entry.takeoff))
      date = entry.takeoff;

  if (!date.IsPlausible())
    date = BrokenDateTime::NowUTC();

  const auto dir = logbook_path.GetParent();
  for (unsigned n = 1;; ++n) {
    const auto name = n == 1
      ? fmt::format("logbook-{:04}-{:02}-{:02}.csv",
                    date.year, date.month, date.day)
      : fmt::format("logbook-{:04}-{:02}-{:02}-{}.csv",
                    date.year, date.month, date.day, n);
    auto dest = AllocatedPath::Build(dir, name.c_str());
    if (File::ExistsAny(dest))
      continue;

    if (!File::Rename(logbook_path, dest))
      throw std::runtime_error("Failed to rename the log book");

    return dest;
  }
}

} // namespace Logbook
