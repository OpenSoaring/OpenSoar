// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Device/Config.hpp"

class ProfileMap;

namespace Profile
{
  void GetDeviceConfig(const ProfileMap &map, unsigned n,
                       DeviceConfig &config);
  void SetDeviceConfig(ProfileMap &map, unsigned n,
                       const DeviceConfig &config);

  /**
   * The name a port type has in a settings file.  Shared with the
   * device port file (Device/PortsConfig), so that both spell the
   * types the same way.
   *
   * @return nullptr if the type is unknown
   */
  [[gnu::const]]
  const char *PortTypeToString(DeviceConfig::PortType type) noexcept;

  [[gnu::pure]]
  bool StringToPortType(const char *value,
                        DeviceConfig::PortType &type) noexcept;
};
