// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <cstdint>

/**
 * What shall happen after the program has closed itself.  Which of
 * these a build offers depends on the target: a Kobo can power itself
 * off, an OpenVario lets its base menu do it, a desktop can only quit
 * or start itself again, and on Android the operating system owns the
 * application lifecycle.
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

[[gnu::pure]]
PowerAction
Get() noexcept;

/**
 * Remember the command line for a later restart.  Called from main()
 * before anything else; a no-op where the platform provides the
 * command line by itself (Windows) or cannot restart (Android).
 */
void
SaveCommandLine(int argc, char **argv) noexcept;

/**
 * Carry out the pending action.  Called after the program has shut
 * down; may replace the running process (restart on POSIX) or never
 * return (reboot).
 *
 * @param exit_code the exit code the program would use otherwise
 * @return the exit code the process should exit with
 */
int
Perform(int exit_code) noexcept;

} // namespace PowerControl
