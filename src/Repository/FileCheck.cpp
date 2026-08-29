// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "FileCheck.hpp"
#include "Language/Language.hpp"
#include "io/FileReader.hxx"
#include "system/Path.hpp"
#include "util/StringAPI.hxx"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>

/**
 * How much of the file is looked at.  Enough for a header, small
 * enough to stay free on any device.
 */
static constexpr std::size_t HEAD_SIZE = 4096;

using Head = std::string_view;

static bool
StartsWith(Head head, std::string_view prefix) noexcept
{
  return head.substr(0, prefix.size()) == prefix;
}

/**
 * A ZIP archive: "PK\003\004".  .cupx, .wpz and .xcm are all ZIP
 * containers.
 */
static bool
IsZip(Head head) noexcept
{
  return StartsWith(head, std::string_view{"PK\x03\x04", 4});
}

static bool
IsGzip(Head head) noexcept
{
  return StartsWith(head, std::string_view{"\x1f\x8b", 2});
}

/**
 * A web page instead of data - the classic result of a moved file or
 * a server that answers every request with a friendly error page.
 */
static bool
IsHTML(Head head) noexcept
{
  /* skip leading whitespace and a possible UTF-8 BOM */
  if (StartsWith(head, std::string_view{"\xef\xbb\xbf", 3}))
    head.remove_prefix(3);

  while (!head.empty() && (head.front() == ' ' || head.front() == '\r' ||
                           head.front() == '\n' || head.front() == '\t'))
    head.remove_prefix(1);

  for (const auto *i : {"<!doctype html", "<html", "<head",
                        "<!doctype>", "<body"}) {
    const std::string_view prefix{i};
    if (head.size() >= prefix.size() &&
        StringIsEqualIgnoreCase(head.data(), i, prefix.size()))
      return true;
  }

  return false;
}

/**
 * Does this look like text at all?  A NUL byte in the first block
 * means it is not.
 */
static bool
IsText(Head head) noexcept
{
  return head.find('\0') == Head::npos;
}

/**
 * The first line that is neither empty nor a comment.
 */
static Head
FirstDataLine(Head head, char comment='*') noexcept
{
  while (!head.empty()) {
    const auto end = head.find('\n');
    Head line = head.substr(0, end);

    if (!line.empty() && line.back() == '\r')
      line.remove_suffix(1);

    if (!line.empty() && line.front() != comment)
      return line;

    if (end == Head::npos)
      break;

    head.remove_prefix(end + 1);
  }

  return {};
}

static bool
LooksLikeCUP(Head head) noexcept
{
  /* the header line, or any data line, has several comma separated
     fields; SeeYou writes "name,code,country,lat,lon,elev,style,..." */
  const auto line = FirstDataLine(head);
  return std::count(line.begin(), line.end(), ',') >= 4;
}

static bool
LooksLikeOpenAir(Head head) noexcept
{
  /* OpenAir records: AC (class), AN (name), AH/AL (limits), DP
     (point), plus "*" comments.  SUA files use "TITLE=" etc. */
  for (const auto *i : {"AC ", "AC\t", "AN ", "AH ", "AL ", "DP ",
                        "TITLE=", "TYPE="})
    if (head.find(i) != Head::npos)
      return true;

  return false;
}

static bool
LooksLikeIGC(Head head) noexcept
{
  return !head.empty() && head.front() == 'A';
}

static bool
LooksLikeXML(Head head) noexcept
{
  while (!head.empty() && (head.front() == ' ' || head.front() == '\r' ||
                           head.front() == '\n' || head.front() == '\t'))
    head.remove_prefix(1);

  return StartsWith(head, "<");
}

FileCheckResult
CheckFileContent(Path path, FileType type) noexcept
{
  char buffer[HEAD_SIZE];
  std::size_t nbytes;

  try {
    FileReader file{path};
    nbytes = file.Read(std::as_writable_bytes(std::span{buffer}));
  } catch (...) {
    return FileCheckResult::UNREADABLE;
  }

  if (nbytes == 0)
    return FileCheckResult::EMPTY;

  const Head head{buffer, nbytes};

  /* a web page is never right, whatever the type says - except of
     course when the type has no idea what it wants (UNKNOWN) */
  if (type != FileType::UNKNOWN && IsHTML(head))
    return FileCheckResult::HTML;

  switch (type) {
  case FileType::WAYPOINT:
    /* .cupx and .wpz are ZIP containers, .cup/.dat/.xcw are text */
    if (IsZip(head))
      return FileCheckResult::OK;

    if (!IsText(head))
      return FileCheckResult::MISMATCH;

    return LooksLikeCUP(head) || FirstDataLine(head).find(',') != Head::npos
      ? FileCheckResult::OK
      : FileCheckResult::MISMATCH;

  case FileType::MAP:
    /* an XCSoar map file is a ZIP archive */
    return IsZip(head)
      ? FileCheckResult::OK
      : FileCheckResult::MISMATCH;

  case FileType::AIRSPACE:
    if (!IsText(head))
      return FileCheckResult::MISMATCH;

    return LooksLikeOpenAir(head)
      ? FileCheckResult::OK
      : FileCheckResult::MISMATCH;

  case FileType::IGC:
    return IsText(head) && LooksLikeIGC(head)
      ? FileCheckResult::OK
      : FileCheckResult::MISMATCH;

  case FileType::TASK:
    /* .tsk is XML, .cup and .igc are allowed here as well */
    if (!IsText(head))
      return FileCheckResult::MISMATCH;

    return LooksLikeXML(head) || LooksLikeCUP(head) || LooksLikeIGC(head)
      ? FileCheckResult::OK
      : FileCheckResult::MISMATCH;

  case FileType::PLANE:
  case FileType::PROFILE:
  case FileType::XCI:
  case FileType::CHECKLIST:
  case FileType::WAYPOINTDETAILS:
  case FileType::LUA:
  case FileType::NMEA:
  case FileType::FLARMDB:
    /* all of these are text of one shape or another; the only thing
       worth reporting is that binary arrived */
    return IsText(head)
      ? FileCheckResult::OK
      : FileCheckResult::MISMATCH;

  case FileType::IMAGE:
    /* OpenVario firmware: *.img.gz */
    return IsGzip(head)
      ? FileCheckResult::OK
      : FileCheckResult::MISMATCH;

  case FileType::FLARMNET:
  case FileType::RASP:
  case FileType::UNKNOWN:
  case FileType::COUNT:
    /* FlarmNet is hex text or a ZIP depending on the source, RASP is
       a binary grid: no signature reliable enough to complain about */
    break;
  }

  return FileCheckResult::UNKNOWN;
}

const char *
GetFileCheckMessage(FileCheckResult result) noexcept
{
  switch (result) {
  case FileCheckResult::OK:
  case FileCheckResult::UNKNOWN:
    return nullptr;

  case FileCheckResult::UNREADABLE:
    return N_("The downloaded file cannot be read.");

  case FileCheckResult::EMPTY:
    return N_("The downloaded file is empty.");

  case FileCheckResult::HTML:
    return N_("A web page was downloaded instead of the file - the "
              "address in the repository is probably wrong.");

  case FileCheckResult::MISMATCH:
    return N_("The downloaded file does not look like the type the "
              "repository says it is.");
  }

  return nullptr;
}

bool
CheckFileName(const char *name, FileType type) noexcept
{
  if (name == nullptr || *name == '\0')
    return false;

  if (type == FileType::UNKNOWN)
    return true;

  const char *patterns = GetFileTypePatterns(type);
  if (patterns == nullptr || *patterns == '\0')
    /* a type without patterns accepts anything */
    return true;

  return FilenameMatchesFileType(name, type);
}
