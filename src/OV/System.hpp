// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "DisplayOrientation.hpp"

#include <map>
#include <string>
#include <string_view>

class Path;

enum class SSHStatus {
  ENABLED,
  DISABLED,
  TEMPORARY,
};
/**
 * Is this program running on an OpenVario?  The OpenVario image keeps
 * its boot settings (display rotation, the program to start) in
 * /boot/config.uEnv, and other Linux systems do not have that file.
 */
[[gnu::pure]]
bool
IsOpenVario() noexcept;

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

SSHStatus
OpenvarioGetSSHStatus();

void
OpenvarioEnableSSH(bool temporary);

void
OpenvarioDisableSSH();

/**
 * The program the OpenVario image starts after boot: the value of
 * "main_app" in /boot/config.uEnv, for example "xcsoar" or
 * "OpenSoar".  Empty if the image has not chosen one yet.
 */
std::string
OpenvarioGetMainApp();

/**
 * Store the program the OpenVario image starts after boot.  It takes
 * effect with the next boot.
 */
void
OpenvarioSetMainApp(std::string_view name);

/**
 * Turn the first line of /boot/image-version-info, the file name of
 * the image ("OV-3.2.20-CB2-CH57.img"), into the name of the image
 * without file suffixes.
 */
constexpr std::string_view
OpenvarioImageName(std::string_view line) noexcept
{
  for (std::string_view suffix : {".gz", ".img"})
    if (line.size() > suffix.size() && line.ends_with(suffix))
      line.remove_suffix(suffix.size());

  return line;
}

/**
 * The name of the image the OpenVario is running, read from
 * /boot/image-version-info.  Empty if that file does not exist.
 */
std::string
OpenvarioGetImageName();
