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

const char *
ToString(LogbookEntry::Launch launch) noexcept
{
  switch (launch) {
  case LogbookEntry::Launch::UNKNOWN:
    break;
  case LogbookEntry::Launch::WINCH:
    return "winch";
  case LogbookEntry::Launch::AEROTOW:
    return "aerotow";
  case LogbookEntry::Launch::SELF:
    return "self";
  }

  return "";
}

LogbookEntry::Launch
ParseLaunch(std::string_view s) noexcept
{
  if (s == "winch")
    return LogbookEntry::Launch::WINCH;
  if (s == "aerotow")
    return LogbookEntry::Launch::AEROTOW;
  if (s == "self")
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
static std::string
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
  PILOT,
  COPILOT,
  AIRCRAFT,
  REGISTRATION,
  COMPETITION_ID,
  FREE_DISTANCE,
  DMST_DISTANCE,
  DMST_POINTS,
  DMST_SHAPE,
  MAX_ALTITUDE,
  IGC_FILE,
  REMARK,
  N_COLUMNS,
};

const char *
GetHeader() noexcept
{
  return "Date;Takeoff (UTC);Landing (UTC);Duration;"
    "Takeoff place;Landing place;Launch;Pilot;Copilot;"
    "Aircraft;Registration;Competition ID;"
    "Free distance (km);DMSt distance (km);DMSt points;DMSt shape;"
    "Max. altitude (m);IGC file;Remark";
}

bool
ParseLine(std::string_view line, LogbookEntry &entry) noexcept
{
  auto columns = SplitLine(line);
  /* older files or a spreadsheet may have dropped empty columns at
     the end */
  columns.resize(N_COLUMNS);

  BrokenDate date;
  BrokenTime takeoff;
  if (!ParseDate(columns[DATE], date) ||
      !ParseTime(columns[TAKEOFF], takeoff))
    /* the header, or not a flight */
    return false;

  entry = {};
  entry.takeoff = BrokenDateTime(date, takeoff);

  if (BrokenTime landing; ParseTime(columns[LANDING], landing)) {
    entry.landing = BrokenDateTime(date, landing);

    /* a flight across midnight UTC */
    if (entry.landing < entry.takeoff)
      entry.landing = entry.landing + std::chrono::hours{24};
  }

  entry.takeoff_place = Strip(std::string_view{columns[TAKEOFF_PLACE]});
  entry.landing_place = Strip(std::string_view{columns[LANDING_PLACE]});
  entry.launch = ParseLaunch(Strip(std::string_view{columns[LAUNCH]}));
  entry.pilot = std::move(columns[PILOT]);
  entry.copilot = std::move(columns[COPILOT]);
  entry.aircraft = std::move(columns[AIRCRAFT]);
  entry.registration = std::move(columns[REGISTRATION]);
  entry.competition_id = std::move(columns[COMPETITION_ID]);
  entry.free_distance = ParseKilometres(columns[FREE_DISTANCE]);
  entry.dmst_distance = ParseKilometres(columns[DMST_DISTANCE]);
  entry.dmst_points = ParseNumber(columns[DMST_POINTS]);
  entry.dmst_shape =
    ParseDMStShape(Strip(std::string_view{columns[DMST_SHAPE]}));

  if (char *end; !columns[MAX_ALTITUDE].empty()) {
    const long value = ParseInt(columns[MAX_ALTITUDE].c_str(), &end);
    if (end != columns[MAX_ALTITUDE].c_str())
      entry.max_altitude = value;
  }

  entry.igc_file = std::move(columns[IGC_FILE]);
  entry.remark = std::move(columns[REMARK]);
  return true;
}

static std::string
FormatKilometres(double metres) noexcept
{
  return metres > 0
    ? fmt::format("{:.1f}", metres / 1000)
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
                             std::string_view{ToString(e.launch)},
                             std::string_view{e.pilot},
                             std::string_view{e.copilot},
                             std::string_view{e.aircraft},
                             std::string_view{e.registration},
                             std::string_view{e.competition_id}}) {
    line += Quote(s);
    line += SEPARATOR;
  }

  line += FormatKilometres(e.free_distance);
  line += SEPARATOR;
  line += FormatKilometres(e.dmst_distance);
  line += SEPARATOR;
  if (e.dmst_points > 0)
    line += fmt::format("{:.1f}", e.dmst_points);
  line += SEPARATOR;
  line += ToString(e.dmst_shape);
  line += SEPARATOR;
  if (e.max_altitude >= 0)
    line += fmt::format("{}", e.max_altitude);
  line += SEPARATOR;
  line += Quote(e.igc_file);
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
  char *line;
  while ((line = reader.ReadLine()) != nullptr) {
    LogbookEntry entry;
    if (ParseLine(line, entry))
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

void
Append(Path path, const LogbookEntry &entry)
{
  const bool is_new = !File::Exists(path);

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
