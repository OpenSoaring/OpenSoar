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
  if (startProfileFile == nullptr)
    SetFiles(nullptr);
  assert(startProfileFile != nullptr);

#ifdef TEMP_FILE_RENAME_ACTION
  auto old_dev_file = LocalPath(DEVICE_MAP);
  if (File::Exists(old_dev_file) &&
    !File::Exists(portSettingFile))
    LoadFile(old_dev_file);
  // File::Rename(old_dev_file, portSettingFile);
#endif

  LogString("Loading profiles");
  LoadFile(startProfileFile);

  if (File::Exists(portSettingFile)) {
    // if portSettingFile exist load port information from this
    LogString("Loading device port information");
    LoadFile(portSettingFile);
  }

  SetModified(false);
}

void
Profile::LoadFile(Path path) noexcept
{
  try {
 
    if (path == portSettingFile) {
      LoadFile(device_ports, path);
    } else if (path == startProfileFile) {
      // normal Profile file
      LoadFile(map, path);
    } else {
#ifdef TEMP_FILE_RENAME_ACTION
      if (!old_dev_file.empty() && File::Exists(old_dev_file))
        LoadFile(device_ports, old_dev_file);
      else
#endif
        LoadFile(map, path);
    }

    if (device_ports.empty()) {
      /* if no device_ports exist, load port information from normal 
       * profile file - and move it to the device_ports */
#if 1
      for (auto setting : map) {
        // auto next = &setting;
        // next++;
        if (setting.first.starts_with("Port") ||
          setting.first.starts_with("Device")) {
          device_ports.Set(setting.first, setting.second.c_str());
          map.Remove(setting.first);
#ifdef _DEBUG
        } else {
          LogFmt("PortSetting: {} = {}", setting.first, setting.second);
#endif
        }
        // setting = *next;
      }
#else
      for (auto setting = map.begin(); setting != map.end(); ) {
        auto next = setting;
        next++;
        if (setting->first.starts_with("Port") || 
          setting->first.starts_with("Device")) {
          device_ports.Set(setting->first, setting->second.c_str());
          map.Remove(setting->first);
#ifdef _DEBUG
        } else {
            LogFmt("PortSetting: {} = {}", setting->first, setting->second);
#endif
        }
        setting = next;
      }
#endif
    }
#if 1  // def _DEBUG
    LogFormat("Loaded profile from %s", path.c_str());
#endif
  } catch (...) {
    LogError(std::current_exception(), "Failed to load profile");
  }
}

void
Profile::Save(const ProfileMap &_map) noexcept
{
  if (!IsModified())
    return;

  Path path = (&_map == &device_ports) ? portSettingFile : startProfileFile;
#if 1  // def _DEBUG
  LogString("Saving profiles");
#endif
  if (path == nullptr)
    SetFiles(nullptr);

  assert(startProfileFile != nullptr);

  try {
    SaveFile(_map, path);
  } catch (...) {
    LogError(std::current_exception(), "Failed to save profile");
  }
}

void
Profile::Save() noexcept
{
  if (!IsModified() && File::Exists(portSettingFile))
    return;

#if 1  // def _DEBUG
  LogString("Saving profiles");
#endif
  if (startProfileFile == nullptr)
    SetFiles(nullptr);

  assert(startProfileFile != nullptr);

  try {
    SaveFile(map, startProfileFile);
    SaveFile(device_ports, portSettingFile);
#ifdef TEMP_FILE_RENAME_ACTION
    // not before saving the new portSettingFile
    old_dev_file = LocalPath(DEVICE_MAP);
    if (File::Exists(old_dev_file))
      File::Delete(old_dev_file);
#endif
  } catch (...) {
    LogError(std::current_exception(), "Failed to save profile");
  }
}

void
Profile::SaveFile(Path path)
{
  // save Profile only (not the port setting!)
  // caller is InputEventsSettings
#if 1  // def _DEBUG
  LogFormat("Saving profile to %s", path.c_str());
#endif
  SaveFile(map, path);
}

void
Profile::SetFiles(Path override_path) noexcept
{
  /* set the "modified" flag, because we are potentially saving to a
     new file now */
  SetModified(true);

  // Set the port profile file
  // portSettingFile = AllocatedPath("D:/Data/OpenSoarData/" DEVICE_PORTS);
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
