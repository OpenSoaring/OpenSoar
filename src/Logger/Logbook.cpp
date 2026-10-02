// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Logbook.hpp"
#include "io/FileLineReader.hpp"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"
#include "util/StringStrip.hxx"
#include "util/NumberParser.hpp"

#include <fmt/format.h>

#include <cmath>
#include <cstdio>

namespace Logbook {

static constexpr char SEPARATOR = ';';

/* the launch is one letter in the file, short enough to keep the
   column narrow and to type by hand in a spreadsheet: Winde/winch,
   Eigenstart (self-launch), F-Schlepp (aerotow), U for unknown, which
   includes a recording that began in flight */
const char *
ToString(LogbookEntry::Launch launch) noexcept
{
  switch (launch) {
  case LogbookEntry::Launch::UNKNOWN:
    break;
  case LogbookEntry::Launch::WINCH:
    return "W";
  case LogbookEntry::Launch::AEROTOW:
    return "F";
  case LogbookEntry::Launch::SELF:
    return "E";
  }

  return "U";
}

LogbookEntry::Launch
ParseLaunch(std::string_view s) noexcept
{
  /* the letters of this version and their English counterparts (S
     for self-launch, A for aerotow), and the words of the first
     version */
  if (s == "W" || s == "w" || s == "winch")
    return LogbookEntry::Launch::WINCH;
  if (s == "F" || s == "f" || s == "A" || s == "a" || s == "aerotow")
    return LogbookEntry::Launch::AEROTOW;
  if (s == "E" || s == "e" || s == "S" || s == "s" || s == "self")
    return LogbookEntry::Launch::SELF;
  return LogbookEntry::Launch::UNKNOWN;
}

const char *
ToString(LogbookStatistics::DMStShape shape) noexcept
{
  switch (shape) {
  case LogbookStatistics::DMStShape::NONE:
    break;
  case LogbookStatistics::DMStShape::QUADRILATERAL:
    return "quadrilateral";
  case LogbookStatistics::DMStShape::TRIANGLE:
    return "triangle";
  case LogbookStatistics::DMStShape::OUT_AND_RETURN:
    return "out-and-return";
  }

  return "";
}

LogbookStatistics::DMStShape
ParseDMStShape(std::string_view s) noexcept
{
  using Shape = LogbookStatistics::DMStShape;
  if (s == "quadrilateral")
    return Shape::QUADRILATERAL;
  if (s == "triangle")
    return Shape::TRIANGLE;
  if (s == "out-and-return")
    return Shape::OUT_AND_RETURN;
  return Shape::NONE;
}

std::vector<std::string>
SplitLine(std::string_view line) noexcept
{
  std::vector<std::string> columns;
  std::string column;
  bool quoted = false;

  for (std::size_t i = 0; i < line.size(); ++i) {
    const char ch = line[i];

    if (quoted) {
      if (ch == '"') {
        if (i + 1 < line.size() && line[i + 1] == '"') {
          /* a doubled quote is one quote in the text */
          column.push_back('"');
          ++i;
        } else
          quoted = false;
      } else
        column.push_back(ch);
    } else if (ch == '"')
      quoted = true;
    else if (ch == SEPARATOR) {
      columns.push_back(std::move(column));
      column.clear();
    } else if (ch != '\r')
      column.push_back(ch);
  }

  columns.push_back(std::move(column));
  return columns;
}

/**
 * Quote a text column if it contains the separator, a quote or a
 * line break, so the spreadsheet keeps it in one cell.
 */
std::string
Quote(std::string_view s) noexcept
{
  if (s.find_first_of(";\"\r\n") == s.npos)
    return std::string{s};

  std::string result{'"'};
  for (const char ch : s) {
    if (ch == '"')
      result.push_back('"');
    /* the file has one flight per line; a line break in a remark
       would split it */
    result.push_back(ch == '\r' || ch == '\n' ? ' ' : ch);
  }

  result.push_back('"');
  return result;
}

static bool
ParseDate(std::string_view s, BrokenDate &date) noexcept
{
  unsigned year, month, day;
  if (std::sscanf(std::string{s}.c_str(), "%u-%u-%u",
                  &year, &month, &day) != 3)
    return false;

  date = BrokenDate(year, month, day);
  return date.IsPlausible();
}

static bool
ParseTime(std::string_view s, BrokenTime &time) noexcept
{
  unsigned hour, minute, second = 0;
  if (std::sscanf(std::string{s}.c_str(), "%u:%u:%u",
                  &hour, &minute, &second) < 2)
    return false;

  time = BrokenTime(hour, minute, second);
  return time.IsPlausible();
}

/**
 * A number with one decimal; 0 if empty or not positive.
 */
static double
ParseNumber(std::string_view s) noexcept
{
  /* a spreadsheet saved in a German locale writes a decimal comma */
  std::string tmp{s};
  for (char &ch : tmp)
    if (ch == ',')
      ch = '.';

  char *end;
  const double value = ParseDouble(tmp.c_str(), &end);
  return end != tmp.c_str() && value > 0 ? value : 0;
}

/**
 * Distances are written in kilometres, kept in metres.
 */
static double
ParseKilometres(std::string_view s) noexcept
{
  return ParseNumber(s) * 1000;
}

/* the columns of the file, in this order */
enum Column : unsigned {
  DATE,
  TAKEOFF,
  LANDING,
  DURATION,
  TAKEOFF_PLACE,
  LANDING_PLACE,
  LAUNCH,
  RELEASE,
  PILOT,
  COPILOT,
  AIRCRAFT,
  REGISTRATION,
  COMPETITION_ID,
  FREE_DISTANCE,
  FREE_SPEED,
  DMST_DISTANCE,
  DMST_POINTS,
  DMST_SHAPE,
  MAX_ALTITUDE,
  LOG_FILE,
  FILE_TYPE,
  RECORDER_CODE,
  RECORDER_TYPE,
  REMARK,
  N_COLUMNS,
};

/**
 * The names of the columns in the header.  A file is read by these
 * names, so a column added later, or one a spreadsheet moved, does not
 * shift the others.
 */
static constexpr const char *column_names[N_COLUMNS] = {
  "Date",
  "Takeoff (UTC)",
  "Landing (UTC)",
  "Duration",
  "Takeoff place",
  "Landing place",
  "Launch",
  "Release (UTC)",
  "Pilot",
  "Copilot",
  "Aircraft",
  "Registration",
  "Competition ID",
  "Free distance (km)",
  "Free speed (km/h)",
  "DMSt distance (km)",
  "DMSt points",
  "DMSt shape",
  "Max. altitude (m)",
  "Log file",
  "File type",
  "Recorder code",
  "Recorder type",
  "Remark",
};

const char *
GetHeader() noexcept
{
  static const std::string header = []{
    std::string h;
    for (const char *name : column_names) {
      if (!h.empty())
        h += SEPARATOR;
      h += name;
    }
    return h;
  }();

  return header.c_str();
}

ColumnMap::ColumnMap() noexcept
{
  static_assert(N_COLUMNS <= MAX_VALUES);

  for (unsigned i = 0; i < N_COLUMNS; ++i)
    index[i] = i;
}

bool
ColumnMap::ParseHeader(std::string_view line) noexcept
{
  const auto names = SplitLine(line);
  if (names.empty() || Strip(std::string_view{names.front()}) != "Date")
    return false;

  for (auto &i : index)
    i = -1;

  for (unsigned c = 0; c < names.size(); ++c) {
    std::string_view name = Strip(std::string_view{names[c]});
    /* the file of the first version called it "IGC file" */
    if (name == "IGC file")
      name = "Log file";

    for (unsigned i = 0; i < N_COLUMNS; ++i)
      if (name == column_names[i])
        index[i] = c;
  }

  return true;
}

bool
ParseLine(std::string_view line, LogbookEntry &entry,
          const ColumnMap &map) noexcept
{
  auto columns = SplitLine(line);

  /* a column the file does not have reads as empty */
  const std::string empty;
  const auto get = [&](Column c) -> const std::string & {
    const int i = map.index[c];
    return i >= 0 && unsigned(i) < columns.size() ? columns[i] : empty;
  };

  BrokenDate date;
  BrokenTime takeoff;
  if (!ParseDate(get(DATE), date) || !ParseTime(get(TAKEOFF), takeoff))
    /* the header, or not a flight */
    return false;

  entry = {};
  entry.takeoff = BrokenDateTime(date, takeoff);

  if (BrokenTime landing; ParseTime(get(LANDING), landing)) {
    entry.landing = BrokenDateTime(date, landing);

    /* a flight across midnight UTC */
    if (entry.landing < entry.takeoff)
      entry.landing = entry.landing + std::chrono::hours{24};
  }

  entry.takeoff_place = Strip(std::string_view{get(TAKEOFF_PLACE)});
  entry.landing_place = Strip(std::string_view{get(LANDING_PLACE)});
  entry.launch = ParseLaunch(Strip(std::string_view{get(LAUNCH)}));

  if (BrokenTime release; ParseTime(get(RELEASE), release)) {
    entry.release = BrokenDateTime(date, release);
    if (entry.release < entry.takeoff)
      entry.release = entry.release + std::chrono::hours{24};
  }
  entry.pilot = get(PILOT);
  entry.copilot = get(COPILOT);
  entry.aircraft = get(AIRCRAFT);
  entry.registration = get(REGISTRATION);
  entry.competition_id = get(COMPETITION_ID);
  entry.free_distance = ParseKilometres(get(FREE_DISTANCE));
  entry.free_speed = ParseNumber(get(FREE_SPEED));
  entry.dmst_distance = ParseKilometres(get(DMST_DISTANCE));
  entry.dmst_points = ParseNumber(get(DMST_POINTS));
  entry.dmst_shape = ParseDMStShape(Strip(std::string_view{get(DMST_SHAPE)}));

  if (const std::string &alt = get(MAX_ALTITUDE); !alt.empty()) {
    char *end;
    const long value = ParseInt(alt.c_str(), &end);
    if (end != alt.c_str())
      entry.max_altitude = value;
  }

  entry.log_file = get(LOG_FILE);
  entry.file_type = get(FILE_TYPE);
  entry.recorder_code = get(RECORDER_CODE);
  entry.recorder_type = get(RECORDER_TYPE);
  entry.remark = get(REMARK);
  return true;
}

/**
 * A number with one decimal and a decimal comma, as a spreadsheet in a
 * German (and most European) locale reads and writes it; the
 * separator of the columns is the semicolon for the same reason.
 */
std::string
FileTypeOf(std::string_view filename) noexcept
{
  const auto dot = filename.rfind('.');
  if (dot == filename.npos)
    return {};

  std::string type{filename.substr(dot + 1)};
  for (char &ch : type)
    if (ch >= 'a' && ch <= 'z')
      ch -= 'a' - 'A';
  return type;
}

static std::string
FormatDecimal(double value) noexcept
{
  std::string s = fmt::format("{:.1f}", value);
  for (char &ch : s)
    if (ch == '.')
      ch = ',';
  return s;
}

static std::string
FormatKilometres(double metres) noexcept
{
  return metres > 0
    ? FormatDecimal(metres / 1000)
    : std::string{};
}

std::string
FormatLine(const LogbookEntry &e) noexcept
{
  std::string line;

  const auto &t = e.takeoff;
  line += fmt::format("{:04}-{:02}-{:02};{:02}:{:02}:{:02};",
                      t.year, t.month, t.day,
                      t.hour, t.minute, t.second);

  if (e.landing.IsPlausible())
    line += fmt::format("{:02}:{:02}:{:02}",
                        e.landing.hour, e.landing.minute,
                        e.landing.second);
  line += SEPARATOR;

  if (e.HasFlightTime()) {
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(e.GetFlightTime()).count();
    line += fmt::format("{}:{:02}", minutes / 60, minutes % 60);
  }
  line += SEPARATOR;

  for (std::string_view s : {std::string_view{e.takeoff_place},
                             std::string_view{e.landing_place},
                             std::string_view{ToString(e.launch)}}) {
    line += Quote(s);
    line += SEPARATOR;
  }

  if (e.release.IsPlausible())
    line += fmt::format("{:02}:{:02}:{:02}",
                        e.release.hour, e.release.minute,
                        e.release.second);
  line += SEPARATOR;

  for (std::string_view s : {std::string_view{e.pilot},
                             std::string_view{e.copilot},
                             std::string_view{e.aircraft},
                             std::string_view{e.registration},
                             std::string_view{e.competition_id}}) {
    line += Quote(s);
    line += SEPARATOR;
  }

  line += FormatKilometres(e.free_distance);
  line += SEPARATOR;
  if (e.free_speed > 0)
    line += FormatDecimal(e.free_speed);
  line += SEPARATOR;
  line += FormatKilometres(e.dmst_distance);
  line += SEPARATOR;
  if (e.dmst_points > 0)
    line += FormatDecimal(e.dmst_points);
  line += SEPARATOR;
  line += ToString(e.dmst_shape);
  line += SEPARATOR;
  if (e.max_altitude >= 0)
    line += fmt::format("{}", e.max_altitude);
  line += SEPARATOR;
  line += Quote(e.log_file);
  line += SEPARATOR;
  line += Quote(e.file_type);
  line += SEPARATOR;
  line += Quote(e.recorder_code);
  line += SEPARATOR;
  line += Quote(e.recorder_type);
  line += SEPARATOR;
  line += Quote(e.remark);
  return line;
}

std::vector<LogbookEntry>
Read(Path path)
{
  std::vector<LogbookEntry> entries;

  if (!File::Exists(path))
    return entries;

  FileLineReaderA reader(path);
  ColumnMap map;
  bool first = true;
  char *line;
  while ((line = reader.ReadLine()) != nullptr) {
    /* the header names the columns; a file without one is read in
       the order of this version */
    if (first) {
      first = false;
      if (map.ParseHeader(line))
        continue;
    }

    LogbookEntry entry;
    if (ParseLine(line, entry, map))
      entries.push_back(std::move(entry));
  }

  return entries;
}

static void
WriteLine(BufferedOutputStream &writer, std::string_view line)
{
  /* CR LF, so the file is also a proper text file for Windows tools */
  writer.Write(line);
  writer.Write("\r\n");
}

/**
 * Does the file begin with the header of this version?
 */
static bool
HasCurrentHeader(Path path)
{
  FileLineReaderA reader(path);
  const char *line = reader.ReadLine();
  return line != nullptr && std::string_view{line} == GetHeader();
}

void
Append(Path path, const LogbookEntry &entry)
{
  const bool is_new = !File::Exists(path);

  if (!is_new && !HasCurrentHeader(path)) {
    /* a file of an older version, or one rearranged in a spreadsheet:
       a line in the current order would not match its header, so the
       whole file is converted to the current columns, read by the
       names in its header */
    auto entries = Read(path);
    entries.push_back(entry);
    Write(path, entries);
    return;
  }

  FileOutputStream file(path, FileOutputStream::Mode::APPEND_OR_CREATE);
  BufferedOutputStream writer(file);

  if (is_new)
    WriteLine(writer, GetHeader());

  WriteLine(writer, FormatLine(entry));

  writer.Flush();
  file.Commit();
}

void
Write(Path path, const std::vector<LogbookEntry> &entries)
{
  /* written to a new file that replaces the old one on Commit(), so
     an error leaves the log book as it was */
  FileOutputStream file(path);
  BufferedOutputStream writer(file);

  WriteLine(writer, GetHeader());
  for (const auto &entry : entries)
    WriteLine(writer, FormatLine(entry));

  writer.Flush();
  file.Commit();
}

} // namespace Logbook
