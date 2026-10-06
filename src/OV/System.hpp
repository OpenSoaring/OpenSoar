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

/**
 * Store the display orientation in /boot/config.uEnv, from where the
 * image applies it to the console at the next boot.
 */
void
OpenvarioSaveRotation(DisplayOrientation orientation);

/**
 * Rotate the display and the console now, and store the orientation
 * for the next boot.
 */
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

/**
 * Where the OpenVario image mounts a USB stick, and where its scripts
 * look for one.
 */
static constexpr const char *openvario_usb_stick = "/usb/usbstick";

/**
 * Does the given content of /proc/mounts show a real file system
 * mounted on #path?  The image mounts the stick through a systemd
 * automount unit, which keeps an "autofs" entry on that path even
 * while no stick is plugged in; only an entry of another type means
 * that the stick is there.
 */
constexpr bool
HasRealMount(std::string_view mounts, std::string_view path) noexcept
{
  while (!mounts.empty()) {
    const auto eol = mounts.find('\n');
    std::string_view line = mounts.substr(0, eol);
    mounts = eol == mounts.npos ? std::string_view{} : mounts.substr(eol + 1);

    /* "device mountpoint type options ..." */
    const auto a = line.find(' ');
    if (a == line.npos)
      continue;
    line.remove_prefix(a + 1);

    const auto b = line.find(' ');
    if (b == line.npos || line.substr(0, b) != path)
      continue;
    line.remove_prefix(b + 1);

    const auto type = line.substr(0, line.find(' '));
    if (!type.empty() && type != "autofs")
      return true;
  }

  return false;
}

/**
 * Is a USB stick mounted where the scripts of the OpenVario image
 * expect it?  Looking into the directory first lets the automount
 * unit mount a stick that has just been plugged in.
 */
bool
OpenvarioIsUsbStickMounted() noexcept;
