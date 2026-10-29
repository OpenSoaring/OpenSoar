// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <cstdint>

/**
 * What shall happen after the program has closed itself.  Which of
 * these a build offers depends on the target: a Kobo can power itself
 * off, an OpenVario lets its base menu do it, a desktop and an
 * Android can only quit.  A restart is possible everywhere, because
 * the program does it within its own process.
 */
enum class PowerAction : uint8_t {
  /** no action pending - the ordinary end of the program */
  NONE,

  /** leave the program */
  QUIT,

  /** leave the program and start it again */
  RESTART,

  /** restart the machine */
  REBOOT,

  /** switch the machine off */
  SHUTDOWN,
};

namespace PowerControl {

/**
 * Is this action available in this build?  Used by the power dialog
 * to decide which buttons to show.
 */
[[gnu::const]]
bool
IsAvailable(PowerAction action) noexcept;

/**
 * Remember the action; it is carried out by Perform() once the
 * program has shut down cleanly (settings are saved by then).
 */
void
Set(PowerAction action) noexcept;

/**
 * Was a restart chosen?  If so, forget it and return true: the caller
 * then runs Main() once more, within the same process, instead of
 * ending it.  This is faster than starting a new process, and it works
 * the same on every target - also where the program cannot start
 * itself (Android) or where the base menu would have to do it
 * (OpenVario).
 */
bool
TakeRestart() noexcept;

/**
 * Carry out the pending action.  Called after the program has shut
 * down for good (a restart was taken by TakeRestart() before); may
 * never return (reboot on the Kobo).
 *
 * @param exit_code the exit code the program would use otherwise
 * @return the exit code the process should exit with
 */
int
Perform(int exit_code) noexcept;

} // namespace PowerControl
