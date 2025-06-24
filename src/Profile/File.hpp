// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

class ProfileMap;
class Path;

namespace Profile {
  /**
   * Throws std::runtime_errror on error.
   * Load the map and marked as not modified
   */
  void LoadFile(ProfileMap &map, Path path);

  /**
   * Throws std::runtime_errror on error.
   * Saved the map and marked as not modified
   */
  void SaveFile(ProfileMap &map, const Path &path);
}
