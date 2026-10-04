// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

/**
 * Show the log book (logbook.csv): one row per flight, newest first;
 * a flight opens with all details, and the pilot can complete the
 * launch, the crew and a remark.
 */
void
ShowLogbookDialog() noexcept;

/**
 * Add the flights of the recorded files (IGC files and NMEA logs) that
 * are not in the log book yet, with a progress dialog that allows
 * cancelling; the rest follows the next time.  Called at startup:
 * the first time, it builds the whole log book.  Does nothing visible
 * if there is nothing to read.
 */
void
UpdateLogbook() noexcept;

/**
 * Ask, then keep the current log book under another name and build it
 * again from all recorded files.
 *
 * @return true if the log book was rebuilt
 */
bool
RebuildLogbook() noexcept;
