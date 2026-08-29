// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Profile.hpp"
#include "Asset.hpp"
#include "Current.hpp"
#include "File.hpp"
#include "LocalPath.hpp"
#include "LogFile.hpp"
#include "SystemConfig.hpp"
#include "Map.hpp"
#include "lib/fmt/PathFormatter.hpp"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"
#include "util/StringAPI.hxx"
#include "util/StringCompare.hxx"
#include "util/StringUtil.hpp"

#include <string>
#include <cassert>
#include <windef.h> /* for MAX_PATH */

#define XCSPROFILE "default.prf"
#define OLDXCSPROFILE "xcsoar-registry.prf"

static AllocatedPath startProfileFile = nullptr;

/** the profile for the next start, if the user picked another one */
static AllocatedPath next_start_profile = nullptr;

static bool loaded = false;

/** set by SetReadOnly(): the file on disk is newer than the memory */
static bool read_only = false;

static AllocatedPath
BuildProfilePath(Path base_name) noexcept
{
  return LocalPath(AllocatedPath::Build(Path("profiles"), base_name));
}

Path
Profile::GetPath() noexcept
{
  return startProfileFile;
}

void
Profile::Load() noexcept
{
  assert(startProfileFile != nullptr);

  LogString("Loading profiles");
  LoadFile(startProfileFile);
  SetModified(false);
  loaded = true;
}

bool
Profile::IsLoaded() noexcept
{
  return loaded;
}

void
Profile::LoadFile(Path path) noexcept
{
  try {
    LoadFile(map, path);
    LogFmt("Loaded profile from {}", path);
  } catch (...) {
    LogError(std::current_exception(), "Failed to load profile");
  }
}

/**
 * The profile marked for the next start shall have the newest
 * timestamp: that is what the startup dialog preselects.
 */
static void
TouchNextStartProfile() noexcept
{
  if (next_start_profile != nullptr &&
      next_start_profile != startProfileFile &&
      !File::Touch(next_start_profile))
    LogFmt("Failed to touch {}", next_start_profile);
}

void
Profile::SetReadOnly() noexcept
{
  LogString("Profile: read-only from now on, restart to reload");
  read_only = true;
}

void
Profile::Save() noexcept
{
  if (!IsModified() || read_only)
    return;

  LogString("Saving profiles");
  if (startProfileFile == nullptr)
    SetFiles(nullptr);

  assert(startProfileFile != nullptr);

  try {
    SaveFile(startProfileFile);
  } catch (...) {
    LogError(std::current_exception(), "Failed to save profile");
  }

  TouchNextStartProfile();
}

bool
Profile::MarkForNextStart(Path path) noexcept
{
  /* whatever this session changed belongs to the running profile:
     write it now, so that no later save gives it a newer timestamp
     than the one picked */
  Save();

  if (!File::Touch(path))
    return false;

  next_start_profile = path;

  /* remember the choice where a timestamp cannot get lost in the
     two-second resolution of a FAT card */
  SystemConfig::Get().last_profile = path.c_str();
  SystemConfig::Save();

  return true;
}

Path
Profile::GetNextStartPath() noexcept
{
  return next_start_profile;
}

void
Profile::SaveFile(Path path)
{
  LogFmt("Saving profile to {}", path);
  SaveFile(map, path);
}

void
Profile::SetFiles(Path override_path) noexcept
{
  /* set the "modified" flag, because we are potentially saving to a
     new file now */
  SetModified(true);

  if (override_path != nullptr) {
    if (override_path.IsBase()) {
      if (StringFind(override_path.c_str(), '.') != nullptr)
        startProfileFile = BuildProfilePath(override_path);
      else {
        std::string t(override_path.c_str());
        t += ".prf";
        startProfileFile = BuildProfilePath(Path(t.c_str()));
      }
    } else
      startProfileFile = Path(override_path);
    return;
  }

  // Set the default profile file
  startProfileFile = BuildProfilePath(Path(XCSPROFILE));
}
