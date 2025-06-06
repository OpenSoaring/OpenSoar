// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

class ProfileMap;

namespace Profile
{
  /**
   * Adjusts the application settings according to the profile settings
   */
  void Use(const ProfileMap &map);
  void UseDevices(ProfileMap &map);
};
