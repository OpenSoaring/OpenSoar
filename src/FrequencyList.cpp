// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "FrequencyList.hpp"
#include "LogFile.hpp"
#include "io/FileReader.hxx"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"
#include "util/IterableSplitString.hxx"
#include "util/StringStrip.hxx"

#include <boost/json.hpp>

#include <cstddef>
#include <span>

/**
 * A frequency list is a hand-written file of a few dozen lines; this
 * is generous enough for any of them and keeps a wrong file (a map,
 * say) from being slurped into memory.
 */
static constexpr std::size_t MAX_FILE_SIZE = 256 * 1024;

/**
 * The plain text list: "name : frequency".  ":" is what OpenSoar
 * always used; "=" and TAB are accepted so that a file written by
 * hand with either of them works as well.
 */
static bool
ParseTextLine(std::string_view line, RadioStation &station) noexcept
{
  line = Strip(line);
  if (line.empty() || line.front() == '#')
    return false;

  auto separator = line.find_first_of(":=\t");
  if (separator == std::string_view::npos)
    return false;

  const auto name = Strip(line.substr(0, separator));
  auto value = Strip(line.substr(separator + 1));
  if (name.empty() || value.empty())
    return false;

  /* a trailing remark after the frequency: "128.950 # FIS" or
     "128.950 FIS" */
  std::string_view comment;
  if (auto space = value.find_first_of(" \t#");
      space != std::string_view::npos) {
    comment = Strip(value.substr(space + 1));
    if (!comment.empty() && comment.front() == '#')
      comment = StripLeft(comment.substr(1));
    value = value.substr(0, space);
  }

  /* "128,950" - a decimal comma, as typed on a German keyboard */
  char buffer[16];
  if (value.size() < sizeof(buffer) &&
      value.find(',') != std::string_view::npos) {
    std::size_t n = 0;
    for (const char ch : value)
      buffer[n++] = ch == ',' ? '.' : ch;
    value = std::string_view{buffer, n};
  }

  const auto frequency = RadioFrequency::Parse(value);
  if (!frequency.IsDefined())
    return false;

  station.name.assign(name);
  station.frequency = frequency;
  station.comment.assign(comment);
  return true;
}

static FrequencyList
ParseText(std::string_view contents) noexcept
{
  FrequencyList list;

  for (const auto line : IterableSplitString(contents, '\n')) {
    RadioStation station;
    if (ParseTextLine(line, station))
      list.emplace_back(std::move(station));
  }

  return list;
}

static const char *
GetString(const boost::json::object &o, const char *key) noexcept
{
  const auto *value = o.if_contains(key);
  if (value == nullptr)
    return nullptr;

  const auto *s = value->if_string();
  return s != nullptr ? s->c_str() : nullptr;
}

/**
 * "frequency" may be written as a string ("128.950") or as a number
 * (128.95); both mean MHz.
 */
static RadioFrequency
GetFrequency(const boost::json::object &o) noexcept
{
  const auto *value = o.if_contains("frequency");
  if (value == nullptr)
    return RadioFrequency::Null();

  if (const auto *s = value->if_string())
    return RadioFrequency::Parse(std::string_view{s->data(), s->size()});

  if (value->is_number()) {
    const double mhz = value->to_number<double>();
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.3f", mhz);
    return RadioFrequency::Parse(buffer);
  }

  return RadioFrequency::Null();
}

static bool
ParseJsonStation(const boost::json::value &value,
                 RadioStation &station) noexcept
{
  const auto *o = value.if_object();
  if (o == nullptr)
    return false;

  const char *name = GetString(*o, "name");
  if (name == nullptr || *name == '\0')
    return false;

  const auto frequency = GetFrequency(*o);
  if (!frequency.IsDefined())
    return false;

  station.name = name;
  station.frequency = frequency;

  if (const char *comment = GetString(*o, "comment"))
    station.comment = comment;

  return true;
}

static FrequencyList
ParseJson(std::string_view contents) noexcept
{
  FrequencyList list;

  boost::json::value root;
  try {
    root = boost::json::parse(contents);
  } catch (...) {
    LogError(std::current_exception(), "Frequency list: JSON error");
    return list;
  }

  /* either { "stations": [ ... ] } or, more forgiving, a bare array
     of stations */
  const boost::json::array *stations = root.if_array();
  if (stations == nullptr)
    if (const auto *o = root.if_object())
      if (const auto *s = o->if_contains("stations"))
        stations = s->if_array();

  if (stations == nullptr)
    return list;

  for (const auto &i : *stations) {
    RadioStation station;
    if (ParseJsonStation(i, station))
      list.emplace_back(std::move(station));
  }

  return list;
}

FrequencyList
ParseFrequencyList(std::string_view contents) noexcept
{
  /* skip a UTF-8 BOM and whitespace, then let the first character
     decide: JSON starts with "{" or "[", everything else is the text
     list */
  if (contents.starts_with("\xef\xbb\xbf"))
    contents.remove_prefix(3);

  const auto head = StripLeft(contents);
  if (!head.empty() && (head.front() == '{' || head.front() == '['))
    return ParseJson(head);

  return ParseText(contents);
}

FrequencyList
LoadFrequencyList(Path path) noexcept
{
  /* no file is the normal state on most devices: not worth a log
     line */
  if (path == nullptr || path.empty() || !File::Exists(path))
    return {};

  std::string contents;

  try {
    FileReader file{path};

    const auto size = file.GetSize();
    if (size > MAX_FILE_SIZE) {
      LogFmt("Frequency list {} is too big to be one", path.c_str());
      return {};
    }

    contents.resize(size);
    std::size_t nbytes = 0;
    while (nbytes < contents.size()) {
      const auto n = file.Read(std::as_writable_bytes(std::span{contents}.subspan(nbytes)));
      if (n == 0)
        break;
      nbytes += n;
    }

    contents.resize(nbytes);
  } catch (...) {
    LogError(std::current_exception(), "Failed to read the frequency list");
    return {};
  }

  return ParseFrequencyList(contents);
}
