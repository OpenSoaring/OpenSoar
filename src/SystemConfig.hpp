// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <string>

/**
 * Settings that belong to this device rather than to a profile or to
 * a data directory.  They live in a file of their own outside the
 * data path (LocalPath: GetSystemConfigPath()), so that they survive
 * a profile change and do not travel when the data directory is
 * copied to another device.
 *
 * The file has the same key=value format as a profile.
 */
namespace SystemConfig {

struct Settings {
  /**
   * Follow upstream XCSoar in those places where OpenSoar deliberately
   * behaves differently: the start screen (fly/simulator prompt,
   * profile dialog with its countdown) and leaving the program (power
   * dialog instead of "Quit program?").
   *
   * OpenSoar's own behaviour is the default; this switch is for
   * everyone who prefers what XCSoar does.
   */
  bool xcsoar_behaviour;

  /**
   * Keep the NMEA devices and their ports in the profile, the way
   * XCSoar does it, instead of in the device port file.  For the rare
   * case where one machine flies with several sets of instruments -
   * a phone in two different gliders, say.
   */
  bool devices_in_profile;

  /**
   * The absolute path of the profile the user chose last - at the
   * start screen, or with "Activate" in the profile list.  The start
   * screen preselects it.  File timestamps are not good enough for
   * that: on a FAT card (OpenVario, SteFly Nav) they only carry two
   * second resolution, so the profile saved on the way out can tie
   * with the one just activated.  Empty means "no choice recorded":
   * the most recently modified profile is preselected then.
   */
  std::string last_profile;

  void SetDefaults() noexcept {
    xcsoar_behaviour = false;
    devices_in_profile = false;
    last_profile.clear();
  }
};

/**
 * Read the settings file.  Call once at startup, after the data path
 * has been initialised; a missing or unreadable file leaves the
 * defaults in place.
 */
void
Load() noexcept;

/**
 * Write the settings file, creating the directory if necessary.
 */
void
Save() noexcept;

Settings &
Get() noexcept;

/**
 * Shortcut for the switch that is asked at several places during
 * startup and shutdown.
 */
[[gnu::pure]]
bool
IsXCSoarBehaviour() noexcept;

} // namespace SystemConfig
