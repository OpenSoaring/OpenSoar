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

enum class SSHStatus {
  ENABLED,
  DISABLED,
  TEMPORARY,
};

enum Buttons {
  LAUNCH_SHELL = 100,
  START_UPGRADE = 111,
};

class OpenVario_Device {
public:
  OpenVario_Device();

  void LoadSettings() noexcept;
  Path GetSystemConfig() noexcept { return system_config; }
  // void SetSystemConfig(Path configfile) noexcept { system_config = configfile; }
  std::map<std::string, std::string, std::less<>> system_map;

  Path GetSettingsConfig() noexcept { return settings_config; }
  // void SetSettingsConfig(Path configfile) noexcept {
  //   settings_config = configfile;
  // }
  std::map<std::string, std::string, std::less<>> settings;

  Path GetUpgradeConfig() noexcept { return upgrade_config; }
  // void SetUpgradeConfig(Path configfile) noexcept {
  //   upgrade_config = configfile;
  // }
  std::map<std::string, std::string, std::less<>> upgrade_map;
#ifdef _WIN32
  // This map is only for Debug purposes on Non-OpenVario systems
  std::map<std::string, std::string, std::less<>> internal_map;
  Path GetInternalConfig() noexcept { return internal_config; }
#endif
  bool
  IsReal() noexcept 
  {
    return is_real;
  }

  struct {
    bool enabled = true;
    unsigned brightness = 100;
    unsigned timeout = 5;
    unsigned rotation = 0;

    unsigned iTest = 0;
  };

  struct { // internal data
    bool sensord = false;
    bool variod = false;
    unsigned ssh = (unsigned) SSHStatus::DISABLED;
  };


private:
  AllocatedPath system_config;    // system config file, in the OV the
                                  // /boot/config.uEnf
  AllocatedPath upgrade_config;   // the config file for upgrading OV
  AllocatedPath settings_config;  // the config file for settings inside
                                // the OpenVarioBaseMenu
#ifdef _WIN32
  // This path is only for Debug purposes on Non-OpenVario systems
  AllocatedPath internal_config;
#endif

  AllocatedPath home_path;
  AllocatedPath data_path;

  bool is_real = false;
};
extern OpenVario_Device ovdevice;

#if !defined(_WIN32) && 0
# define DBUS_FUNCTIONS 1
#endif

class Path;

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

SSHStatus OpenvarioGetSSHStatus();
void OpenvarioSetSSHStatus(SSHStatus state);
bool OpenvarioGetSensordStatus() noexcept;
bool OpenvarioGetVariodStatus() noexcept;
void OpenvarioSetSensordStatus(bool value) noexcept;
void OpenvarioSetVariodStatus(bool value) noexcept;
