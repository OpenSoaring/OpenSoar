// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

/**
 * Process/application exit codes used by the OpenVario base menu to
 * communicate the next action (start shell, run an upgrade, ...) to
 * the wrapper script that launched the process.
 */
enum ExitValues {
  EXIT_NORMAL = 100,
  EXIT_SYSTEM = 200,
  EXIT_REBOOT = 201,
  EXIT_SHUTDOWN = 202,
#ifdef IS_OPENVARIO
  LAUNCH_SHELL = 203,
  LAUNCH_SHELL_STOP = 204,
  START_UPGRADE = 205,
  LAUNCH_TOUCH_CALIBRATE = 206,
  EXIT_BASE_MENU = 207,
#endif
  EXIT_RESTART = 208,
#ifdef IS_OPENVARIO
  EXIT_NEWSTART = 209,
#endif
};
