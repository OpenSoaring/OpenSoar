// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <cstdint>

enum class FileType : uint8_t {
  UNKNOWN,
  AIRSPACE,
  WAYPOINT,
  WAYPOINTDETAILS,
  MAP,
  FLARMNET,
  IGC,
#ifdef HAVE_RASP
  RASP,
#endif
  XCI,
  TASK,
  IMAGE,
  FREQUENCIES,
  PROFILE,
};
