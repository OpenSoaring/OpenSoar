// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "DisplayOrientation.hpp"
#include "Language/Language.hpp"
#include "system/Path.hpp"

#include <tchar.h>

#include <map>
#include <string>


// extern constexpr const Path ConfigFile;
// constexpr const Path ConfigFile(_T("/boot/config.uEnv"));
extern Path ConfigFile;

class OpenVarioDevice {
public:
  OpenVarioDevice();
  Path GetConfigFile() noexcept 
  {
    return ConfigFile;
  }
  void SetConfigFile(Path _ConfigFile) noexcept 
  {
    ConfigFile = _ConfigFile;
  }

private:
  AllocatedPath ConfigFile;
  AllocatedPath HomePath;
  AllocatedPath DataPath;
};
extern OpenVarioDevice ovdevice;

#if !defined(_WIN32) && 1
# define DBUS_FUNCTIONS 1
#endif

class Path;

enum class SSHStatus {
  ENABLED,
  DISABLED,
  TEMPORARY,
};

enum Buttons {
  LAUNCH_SHELL = 100,
  START_UPGRADE = 111,
};

/**
 * Load a system config file and put its variables into a map
*/
void
LoadConfigFile(std::map<std::string, std::string, std::less<>> &map, Path path);
/**
 * Save a map of config variables to a system config file
*/
void
WriteConfigFile(std::map<std::string, std::string, std::less<>> &map, Path path);

uint_least8_t
OpenvarioGetBrightness() noexcept;

void
OpenvarioSetBrightness(uint_least8_t value) noexcept;

DisplayOrientation
OpenvarioGetRotation();

void
OpenvarioSetRotation(DisplayOrientation orientation);

#ifdef  DBUS_FUNCTIONS
SSHStatus
OpenvarioGetSSHStatus();

void
OpenvarioEnableSSH(bool temporary);

void
OpenvarioDisableSSH();
#endif

void 
GetConfigInt(const std::string &keyvalue, unsigned &value,
             const Path &ConfigPath);

void 
ChangeConfigInt(const std::string &keyvalue, int value,
                const Path &ConfigPath);

