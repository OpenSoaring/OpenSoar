// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "DisplayOrientation.hpp"
#include "Language/Language.hpp"
#include "system/Path.hpp"

#include <tchar.h>

#include <map>
#include <string>


void debugln(const char *fmt, ...) noexcept;

class OpenVarioDevice {
public:
  OpenVarioDevice();

  Path GetSystemConfig() noexcept 
  { 
    return system_config; 
   }

  void SetSystemConfig(Path configfile) noexcept 
  {
    system_config = configfile;
  }

  Path 
  GetSettingsConfig() noexcept 
  {
    return settings_config;
  }
  void 
  SetSettingsConfig(Path configfile) noexcept 
  {
    settings_config = configfile;
  }

  Path 
  GetUpgradeConfig() noexcept 
  {
    return upgrade_config;
  }
  void 
  SetUpgradeConfig(Path configfile) noexcept 
  {
    upgrade_config = configfile;
  }

  bool
  IsReal() noexcept 
  {
    return is_real;
  }

private:
  AllocatedPath system_config;    // system config file, in the OV the
                                  // /boot/config.uEnf
  AllocatedPath upgrade_config;   // the config file for upgrading OV
  AllocatedPath settings_config;  // the config file for settings inside
                                // the OpenVarioBaseMenu
  AllocatedPath home_path;
  AllocatedPath data_path;

  bool is_real = false;
};
extern OpenVarioDevice ovdevice;

#if !defined(_WIN32) && 0
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

