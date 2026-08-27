// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "system/Path.hpp"

void CalibrateSensors() noexcept;

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
