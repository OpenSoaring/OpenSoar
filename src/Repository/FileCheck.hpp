// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "FileType.hpp"

class Path;

/**
 * What a downloaded file turned out to be.  A repository is a hand
 * maintained text file on somebody's web server, and the program has
 * no way of knowing whether the person who wrote it made a mistake -
 * so it looks at what actually arrived instead of trusting the entry.
 */
enum class FileCheckResult {
  /** the content matches the declared type */
  OK,

  /** nothing is known about this type: do not complain */
  UNKNOWN,

  /** the file could not be read at all */
  UNREADABLE,

  /** the file is empty */
  EMPTY,

  /** a web page (usually a "not found" page) instead of data */
  HTML,

  /** readable, but not what the declared type looks like */
  MISMATCH,
};

/**
 * Look at the beginning of a file and decide whether it can be what
 * the repository says it is.  Deliberately generous: it only reports
 * MISMATCH where the format has a signature that cannot be mistaken.
 */
[[gnu::pure]]
FileCheckResult
CheckFileContent(Path path, FileType type) noexcept;

/**
 * A message for the user, or nullptr when there is nothing to
 * complain about (OK and UNKNOWN).  The text is translated.
 */
[[gnu::const]]
const char *
GetFileCheckMessage(FileCheckResult result) noexcept;

/**
 * Does this file name carry an extension the type accepts?  Unlike
 * FilenameMatchesFileType() this tolerates a type without patterns,
 * and is meant for checking a repository entry before downloading it.
 */
[[gnu::pure]]
bool
CheckFileName(const char *name, FileType type) noexcept;
