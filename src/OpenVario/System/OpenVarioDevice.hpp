// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "DisplayOrientation.hpp"
#include "Language/Language.hpp"
#include "system/Path.hpp"
// #include "LogFile.hpp"

#include <tchar.h>

#include <map>
#include <string>
#include <filesystem>


#define DEBUG_TEST_VERSION 1
#if !DEBUG_TEST_VERSION
#define RELEASE_VERSION
#endif


#define DEBUG_OPENVARIO  1
#if __GNUC__ &&  __has_include("dbus/dbus.h")
# define DBUS_FUNCTIONS 1
# warning (Attention: DBUS is enabled)
#elif __GNUC__
# warning (Attention: No DBUS!)
#else
#pragma message("Attention: No DBUS!")
#endif

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
  OpenVario_Device() {}

  void Initialise() noexcept;
  void Deinitialise() noexcept;
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
#ifndef DBUS_FUNCTIONS
  // This map is only for Debug purposes on Non-OpenVario systems
  std::map<std::string, std::string, std::less<>> internal_map;
  Path GetInternalConfig() noexcept { return internal_config; }
#endif
  bool
  IsReal() noexcept 
  {
    return is_real;
  }
  Path GetDataPath() noexcept { return data_path; }
  Path GetHomePath() noexcept { return home_path; }
  std::filesystem::path GetBinPath() noexcept { return bin_path; }
  std::filesystem::path GetExePath() noexcept { return exe_path; }
  std::filesystem::path GetExeName() noexcept {
    return exe_path.filename().replace_extension();
  }
  void SetBinPath(const char *path) noexcept {
    exe_path = path;
    // std::filesystem::path::path 
    bin_path = exe_path.parent_path();
  }
  Path GetRunTempFile() noexcept { return run_output_file; }

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


  uint_least8_t GetBrightness() noexcept;
  void SetBrightness(uint_least8_t value) noexcept;
  DisplayOrientation GetRotation();
  void SetRotation(DisplayOrientation orientation);

  bool GetSystemStatus(std::string_view system) noexcept;
  void SetSystemStatus(std::string_view system, bool value) noexcept;
  SSHStatus GetSSHStatus() noexcept;
  void SetSSHStatus(SSHStatus state) noexcept;

  private:
  AllocatedPath system_config;    // system config file, in the OV the
                                  // /boot/config.uEnf
  AllocatedPath upgrade_config;   // the config file for upgrading OV
  AllocatedPath settings_config;  // the config file for settings inside
                                // the OpenVarioBaseMenu
  AllocatedPath run_output_file; // the temp file in Run() processes
//  AllocatedPath run_output_file;     // the temp file in Run() processes
#ifndef DBUS_FUNCTIONS
  // This path is only for Debug purposes on Non-OpenVario systems
  AllocatedPath internal_config;
#endif

  AllocatedPath home_path;
  AllocatedPath data_path;

  bool is_real = false;
  bool initialised = false;
#if 1  // test with filesystem
  std::filesystem::path exe_path; 
  std::filesystem::path bin_path;
#else
  AllocatedPath exe_path;
  AllocatedPath bin_path;
#endif
};
extern OpenVario_Device ovdevice;

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
