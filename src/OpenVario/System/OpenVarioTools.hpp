// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "system/Path.hpp"

void CalibrateSensors() noexcept;

/**
 * Leave the program with the given exit value so that the wrapper
 * script (ovmenu-ng.sh) runs the next step: an upgrade, the touch
 * calibration, a shell.  Works from inside open dialogs, which
 * UIActions::SignalShutdown() alone does not: with a dialog open,
 * closing the main window only cancels that dialog.  On a PC there is
 * no wrapper and the value is simply the exit status, so the same
 * call lets a development build rehearse the device flow.
 */
void ExitToWrapper(unsigned exit_value) noexcept;

/**
 * Launch a child process, redirect its stdout into @p output_file and
 * wait for it to exit (clean replacement for the old-world
 * Run(Path, ...) overload of system/Process.hpp).
 *
 * @return the child's exit status, or -1 on error
 */
int
RunCapture(Path output_file, const char *const *argv) noexcept;

template<typename... Args>
static inline int
RunCapture(Path output_file, const char *path, Args... args) noexcept
{
  const char *const argv[]{path, args..., nullptr};
  return RunCapture(output_file, argv);
}
