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

#include <windef.h> /* for MAX_PATH */
#include <cassert>

#define XCSPROFILE "default.prf"
#define DEVICE_MAP "device_map.map"
#define OLDXCSPROFILE "xcsoar-registry.prf"

static AllocatedPath startProfileFile = nullptr;
static AllocatedPath portSettingFile = nullptr;

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

  LogString("Loading profiles");
  LoadFile(startProfileFile);

  if (File::Exists(portSettingFile)) {
    // if portSettingFile exist load port information from this
    LogString("Loading device port information");
    LoadFile(portSettingFile);
  }

  if (File::Exists(portSettingFile))
      LoadFile(portSettingFile);
  SetModified(false);
}

void
Profile::LoadFile(Path path) noexcept
{
  try {
    if (path == portSettingFile)
       LoadFile(device_map, path);
    else
       LoadFile(map, path);
    if (device_map.empty()) {
      /* if no device_map exist, load port information from normal 
       * profile file - and move it to the device_map */
      for (auto setting = map.begin(); setting != map.end(); ) {
        auto next = setting;
        next++;
        if (setting->first.starts_with("Port") || 
          setting->first.starts_with("Device")) {
          device_map.Set(setting->first, setting->second.c_str());
          map.Remove(setting->first);
        }
        setting = next;
      }
      SaveFile(device_map, portSettingFile);
    }
    LogFormat("Loaded profile from %s", path.c_str());
  } catch (...) {
    LogError(std::current_exception(), "Failed to load profile");
  }
}

void
Profile::Save() noexcept
{
  if (!IsModified())
    return;

  LogString("Saving profiles");
  if (startProfileFile == nullptr)
    SetFiles(nullptr);

  assert(startProfileFile != nullptr);

  try {
    SaveFile(map, startProfileFile);
    SaveFile(device_map, portSettingFile);
  } catch (...) {
    LogError(std::current_exception(), "Failed to save profile");
  }
}

void
Profile::SaveFile(Path path)
{
  // save Profile only (not the port setting!)
  LogFormat("Saving profile to %s", path.c_str());
  SaveFile(map, path);
}

void
Profile::SetFiles(Path override_path) noexcept
{
  /* set the "modified" flag, because we are potentially saving to a
     new file now */
  SetModified(true);

  // Set the port profile file
  // portSettingFile = AllocatedPath("D:/Data/OpenSoarData/" DEVICE_MAP);
  portSettingFile = LocalPath(DEVICE_MAP);

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
