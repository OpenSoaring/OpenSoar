// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Device/Config.hpp"
#include "Device/Features.hpp"

#include <array>

class Path;
class AllocatedPath;
struct SystemSettings;

/**
 * The NMEA devices and their ports belong to the device, not to the
 * pilot's profile: which instrument hangs on which port is a property
 * of the hardware in front of the pilot.  They are therefore kept in
 * a file of their own next to the other device settings
 * (SystemConfig), in JSON - a format that will not have to be
 * converted again.
 *
 * Everything that writes device settings goes through Save() here;
 * there is deliberately no second way to store them, because a second
 * way is how settings end up in the file nobody reads.
 */
namespace DevicePorts {

/**
 * The name is the one OpenSoar has always used for this file - in the
 * data directory back then, in the device settings directory now, and
 * JSON instead of key=value.
 */
inline constexpr char FILE_NAME[] = "device_ports.xcd";

/**
 * The device port file, or nullptr if this system has no place for
 * device settings.
 */
[[gnu::pure]]
AllocatedPath
GetPath() noexcept;

/**
 * Read the device configuration.
 *
 * @return false if there is no file yet (or it is unreadable); the
 * array is then untouched
 */
bool
Load(std::array<DeviceConfig, NUMDEV> &devices) noexcept;

/**
 * Write the device configuration of all slots.
 *
 * @return false if it could not be written
 */
bool
Save(const std::array<DeviceConfig, NUMDEV> &devices) noexcept;

/**
 * Read an old OpenSoar device_ports.xcd (the key=value format of that
 * time, with "Port1Driver" instead of "DeviceA"), so that an
 * installation from those days keeps its ports.
 *
 * @return false if the file is missing, unreadable or empty
 */
bool
LoadLegacy(Path path, std::array<DeviceConfig, NUMDEV> &devices) noexcept;

/**
 * Called after the profile was loaded: unless the devices are
 * configured to live in the profile, the values from the device port
 * file replace what the profile provided.  Without such a file yet,
 * the profile's devices are taken over into one (migration), so that
 * an existing installation keeps its ports.
 */
void
Apply(SystemSettings &settings) noexcept;

/**
 * Store the configuration of one device slot, wherever the devices
 * live, and write it out immediately.  The caller has already put the
 * value into the live SystemSettings.
 */
void
SaveOne(unsigned index, const DeviceConfig &config) noexcept;

} // namespace DevicePorts
