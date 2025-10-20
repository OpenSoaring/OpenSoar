// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Profile.hpp"
#include "Map.hpp"
#include "File.hpp"
#include "Current.hpp"
#include "LogFile.hpp"
#include "Asset.hpp"
#include "LocalPath.hpp"
#include "util/StringUtil.hpp"
#include "util/StringCompare.hxx"
#include "util/StringAPI.hxx"
#include <string>
#include "system/FileUtil.hpp"
#include "system/Path.hpp"

#include "Device/Config.hpp"
#include "Interface.hpp"
#include "Profile/DeviceConfig.hpp"

#include <windef.h> /* for MAX_PATH */
#include <cassert>

#define XCSPROFILE "default.prf"
#define DEVICE_PORTS "device_ports.xcd"
#define OLDXCSPROFILE "xcsoar-registry.prf"

#define TEMP_FILE_RENAME_ACTION
#ifdef TEMP_FILE_RENAME_ACTION
# define DEVICE_MAP "device_map.map"
#endif

static AllocatedPath startProfileFile = nullptr;
static AllocatedPath portSettingFile = nullptr;
// static AllocatedPath configSettingFile = nullptr;
#ifdef TEMP_FILE_RENAME_ACTION
static AllocatedPath old_dev_file;
#endif
Path
Profile::GetPath() noexcept
{
  return startProfileFile;
}

void
Profile::Load() noexcept
{
#ifdef _DEBUG
  LogFmt("LoadProfile {} ", Profile::GetPath().c_str());
#endif
  if (startProfileFile == nullptr)
    SetFiles(nullptr);
  assert(startProfileFile != nullptr);

#ifdef TEMP_FILE_RENAME_ACTION
  auto old_dev_file = LocalPath(DEVICE_MAP);
  if (File::Exists(old_dev_file)) {
    if (!File::Exists(portSettingFile))
      File::Rename(old_dev_file, portSettingFile);
    else
      File::Delete(old_dev_file);
  }
#endif

  if (File::Exists(portSettingFile)) {
    // if portSettingFile exist load port information from this
#ifdef _DEBUG
    LogString("Loading device port information");
#endif
    LoadFile(portSettingFile);
  }

#ifdef _DEBUG
  LogString("Loading profile information");
#endif
  LoadFile(startProfileFile);

  SetModified(false);
}

namespace Profile {
static void
MovePortSettings() noexcept
{
  /* if no device_ports exist, load port information from normal
   * profile file - and move it to the device_ports */
  for (auto setting : map) { // don't use this with 'Remove'!
    if (setting.first.starts_with("Port")) {
      if (std::isdigit(setting.first[4]))
         device_ports.Set(setting.first, setting.second.c_str());
      else {
        std::string first = "Port1" + setting.first.substr(4);
        device_ports.Set(first, setting.second.c_str());
      }
    } if (setting.first.starts_with("Device")) {
      std::string first = "PortXDriver";
      first[4] = setting.first[6] - 'A' + '1';  // set the correct port
      device_ports.Set(first, setting.second.c_str());
    }
  }
  device_ports.SetModified(true);
}
};

void
Profile::LoadFile(Path path) noexcept
{
  try {
    if (path == portSettingFile) {
      LoadFile(device_ports, path);
    } else if (path == startProfileFile) {
      // normal Profile file
      LoadFile(map, path);
      if (device_ports.empty()) 
        MovePortSettings();
    } else if (path.str().starts_with("test/data")) {
      LoadFile(map, path);  // test-data
    } else {
      LogFmt("LoadFile with wrong file: {}!", path.c_str());
    }
  } catch (...) {
    LogError(std::current_exception(), "Failed to load profile");
  }
}

void
Profile::Save(ProfileMap &_map) noexcept
{
  if (!map.IsModified())
    return;

  Path path = (&_map == &device_ports) ? portSettingFile : startProfileFile;
#ifdef _DEBUG
  LogString("Saving profiles");
#endif
  if (path == nullptr)
    SetFiles(nullptr);

  assert(startProfileFile != nullptr);

  try {
    SaveFile(_map, path);
    _map.SetModified(false);
  } catch (...) {
    LogError(std::current_exception(), "Failed to save profile");
  }
}

void
Profile::Save() noexcept
{ 
  if (!IsModified())
    return;

#ifdef _DEBUG
  LogString("Saving profiles");
#endif
  if (startProfileFile == nullptr)
    SetFiles(nullptr);

  assert(startProfileFile != nullptr);

  try {
    if (map.IsModified())
      SaveFile(map, startProfileFile);
    if (device_ports.IsModified())
      SaveFile(device_ports, portSettingFile);
    SetModified(false);  // set both to 'saved'
  } catch (...) {
    LogError(std::current_exception(), "Failed to save profile");
  }
}

void
Profile::SaveFile(Path path)
{
  // save Profile only (not the port setting!)
  // caller could be InputEventsSettings
#ifdef _DEBUG
  LogFmt("Saving profile to {}", path.c_str());
#endif
  SaveFile(map, path);
}

void
Profile::SetFiles(Path override_path) noexcept
{
  // Set the port profile file
  portSettingFile = LocalPath(DEVICE_PORTS);

  if (override_path != nullptr) {
    if (override_path.IsBase()) {
      if (StringFind(override_path.c_str(), '.') != nullptr)
        startProfileFile = LocalPath(override_path);
      else {
        std::string t(override_path.c_str());
        t += ".prf";
        startProfileFile = LocalPath(t.c_str());
      }
    } else
      startProfileFile = Path(override_path);
    return;
  }

  // Set the default profile file
  startProfileFile = LocalPath(XCSPROFILE);
}

AllocatedPath
Profile::GetPath(std::string_view key) noexcept
{
  return map.GetPath(key);
}

bool
Profile::GetPathIsEqual(std::string_view key, Path value) noexcept
{
  return map.GetPathIsEqual(key, value);
}

void
Profile::SetPath(std::string_view key, Path value) noexcept
{
  map.SetPath(key, value);
}

void
Profile::LoadConfiguration() noexcept
{
  auto path = GetCachePath("system_config.xcc");

  if (File::Exists(path))
      LoadFile(system_config, path);
}

void
Profile::SaveConfiguration() noexcept
{
  

}

AllocatedPath
Profile::GetConfigPath(std::string_view key) noexcept
{
  return system_config.GetPath(key);
}

