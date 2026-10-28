// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "PortsConfig.hpp"
#include "Interface.hpp"
#include "LocalPath.hpp"
#include "LogFile.hpp"
#include "Profile/Current.hpp"
#include "Profile/DeviceConfig.hpp"
#include "Profile/Profile.hpp"
#include "SystemConfig.hpp"
#include "SystemSettings.hpp"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"

/*
 * Where the device settings come from and where they go.  Kept apart
 * from the file itself (PortsConfig.cpp), which knows nothing about
 * the running program and can therefore be tested on its own.
 */

/**
 * Correct the Windows serial port names of older settings ("COM3:",
 * "\\.\COM12") to the spelling OpenSoar uses, "COMn", without asking:
 * they name the same port, and only one spelling lets the port picker
 * find the configured port in its list.
 *
 * @param in_profile also write the corrected slots into the profile
 * @return true if a slot was changed, so that the caller saves it
 */
static bool
NormalizeSerialPaths(std::array<DeviceConfig, NUMDEV> &devices,
                     bool in_profile) noexcept
{
  bool changed = false;
  for (unsigned i = 0; i < devices.size(); ++i) {
    if (!devices[i].NormalizeSerialPath())
      continue;

    LogFmt("Device ports: slot {} now on {}", i, devices[i].path.c_str());
    if (in_profile)
      Profile::SetDeviceConfig(Profile::map, i, devices[i]);
    changed = true;
  }

  return changed;
}

void
DevicePorts::Apply(SystemSettings &settings) noexcept
{
  if (SystemConfig::Get().devices_in_profile) {
    LogString("Device ports: profile");
    if (NormalizeSerialPaths(settings.devices, true))
      Profile::Save();
    return;
  }

  if (Load(settings.devices)) {
    const auto path = GetPath();
    LogFmt("Device ports: {}", path.c_str());
    if (NormalizeSerialPaths(settings.devices, false))
      Save(settings.devices);
    return;
  }

  /* an old OpenSoar kept the ports in the data directory, in the
     key=value format of that time - take those over and write them
     out as JSON, where they belong from now on */
  const auto legacy_path = LocalPath(Path{DevicePorts::FILE_NAME});
  if (File::Exists(legacy_path) &&
      LoadLegacy(legacy_path, settings.devices)) {
    NormalizeSerialPaths(settings.devices, false);
    if (Save(settings.devices))
      LogFmt("Device ports: converted from {}", legacy_path.c_str());
    else
      LogFmt("Device ports: read from {}, but not converted",
             legacy_path.c_str());
    return;
  }

  /* no file at all: the profile's devices - just loaded into
     "settings" - become the first content of the device port file, so
     that an existing installation keeps its ports */
  NormalizeSerialPaths(settings.devices, false);
  if (Save(settings.devices))
    LogString("Device ports: taken over from the profile");
  else
    LogString("Device ports: no file, using the profile");
}

void
DevicePorts::SaveOne(unsigned index, const DeviceConfig &config) noexcept
{
  if (SystemConfig::Get().devices_in_profile) {
    Profile::SetDeviceConfig(Profile::map, index, config);
    Profile::Save();
    return;
  }

  /* the caller has already stored the value in the live settings;
     write out all slots, because the file holds them together */
  Save(CommonInterface::GetSystemSettings().devices);
}
