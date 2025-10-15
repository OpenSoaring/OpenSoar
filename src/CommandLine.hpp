// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <string>

class Args;

struct start_settings {
  std::string_view datapath;
  std::string_view profilepath;
};

namespace CommandLine {
  extern unsigned width, height;

#ifdef KOBO
  static constexpr bool full_screen = false;
#elif defined(ENABLE_SDL) || defined(USE_X11)
#define HAVE_CMDLINE_FULLSCREEN
  extern bool full_screen;
#else
  static constexpr bool full_screen = false;
#endif

#if defined(__linux__) && !defined(ANDROID)
#define HAVE_CMDLINE_REPLAY
  extern const char *replay_path;
#endif

/**
 * Reads and parses arguments/options from the command line
 * @param CommandLine command line argument string
 * fill the struct start_settings with datapath and profilepath
 */
  void Parse(Args &args, start_settings &s);
}
