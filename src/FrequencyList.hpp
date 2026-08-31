// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "RadioFrequency.hpp"

#include <string>
#include <vector>

class Path;

/**
 * One entry of the frequency list: a name to show, the frequency to
 * tune, and an optional remark.
 */
struct RadioStation {
  std::string name;
  RadioFrequency frequency;
  std::string comment;
};

using FrequencyList = std::vector<RadioStation>;

/**
 * Read a frequency list file.  Two formats are understood, told
 * apart by the first character of the file, not by its name:
 *
 * - JSON (the file starts with "{"):
 *
 *     { "stations": [
 *         { "name": "Stuttgart Info", "frequency": "128.950",
 *           "comment": "FIS" },
 *         { "name": "Start", "frequency": 129.890 } ] }
 *
 *   "frequency" may be a string or a number in MHz; "comment" is
 *   optional.
 *
 * - the plain text list OpenSoar has used all along: one station per
 *   line, name and frequency separated by ":" (or "=" or a TAB),
 *   empty lines and lines starting with "#" ignored:
 *
 *     # Wettbewerb 2026
 *     Stuttgart Info : 128.950
 *     Start          : 129.890
 *
 * Entries whose frequency does not parse are skipped, so one typo
 * does not hide the whole list.
 *
 * @return the stations in file order; empty if the file is missing,
 * unreadable or contains no usable entry
 */
FrequencyList
LoadFrequencyList(Path path) noexcept;

/**
 * Parse the contents of a frequency list (see LoadFrequencyList()).
 * Exposed for the unit test.
 */
FrequencyList
ParseFrequencyList(std::string_view contents) noexcept;
