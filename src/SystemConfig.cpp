// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "SystemConfig.hpp"
#include "LocalPath.hpp"
#include "LogFile.hpp"
#include "Profile/File.hpp"
#include "Profile/Map.hpp"
#include "system/Path.hpp"

/**
 * The file name inside GetSystemConfigPath().  It uses the profile
 * format, but it is deliberately not a profile: nothing here is ever
 * copied along with the data directory.
 */
static constexpr char SYSTEM_CONFIG_FILE[] = "system.prf";

static constexpr char KEY_XCSOAR_BEHAVIOUR[] = "XCSoarBehaviour";
static constexpr char KEY_DEVICES_IN_PROFILE[] = "DevicesInProfile";
static constexpr char KEY_LAST_PROFILE[] = "LastProfile";

static SystemConfig::Settings the_config = {
  .xcsoar_behaviour = false,
  .devices_in_profile = false,
  .last_profile = {},
};

[[gnu::pure]]
static AllocatedPath
GetSystemConfigFile() noexcept
{
  const Path dir = GetSystemConfigPath();
  if (dir == nullptr)
    return nullptr;

  return AllocatedPath::Build(dir, Path{SYSTEM_CONFIG_FILE});
}

void
SystemConfig::Load() noexcept
{
  the_config.SetDefaults();

  const auto path = GetSystemConfigFile();
  if (path == nullptr)
    return;

  ProfileMap map;

  try {
    Profile::LoadFile(map, path);
  } catch (...) {
    /* no settings yet, or unreadable: the defaults apply */
    return;
  }

  map.Get(KEY_XCSOAR_BEHAVIOUR, the_config.xcsoar_behaviour);
  map.Get(KEY_DEVICES_IN_PROFILE, the_config.devices_in_profile);
  map.Get(KEY_LAST_PROFILE, the_config.last_profile);
}

void
SystemConfig::Save() noexcept
{
  if (MakeSystemConfigPath() == nullptr) {
    LogString("Cannot create the system configuration directory");
    return;
  }

  const auto path = GetSystemConfigFile();
  if (path == nullptr)
    return;

  ProfileMap map;
  map.Set(KEY_XCSOAR_BEHAVIOUR, the_config.xcsoar_behaviour);
  map.Set(KEY_DEVICES_IN_PROFILE, the_config.devices_in_profile);
  map.Set(KEY_LAST_PROFILE, the_config.last_profile.c_str());

  try {
    Profile::SaveFile(map, path);
  } catch (...) {
    LogError(std::current_exception(), "Failed to save the system configuration");
  }
}

SystemConfig::Settings &
SystemConfig::Get() noexcept
{
  return the_config;
}

bool
SystemConfig::IsXCSoarBehaviour() noexcept
{
  return the_config.xcsoar_behaviour;
}
