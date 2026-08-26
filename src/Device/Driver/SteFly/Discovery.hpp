// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

/** \file
 *
 * Startup-time USB / serial discovery for the SteFly device family
 * (RemoteStick, RotaryPanel).
 *
 * The result is one COM / tty path (e.g. "COM7" on Windows or
 * "/dev/ttyACM0" on Linux) which Startup.cpp uses to pre-configure
 * the fixed REMOTE_PORT slot in SystemSettings::devices before
 * devStartup() opens the ports. This slot is never persisted to
 * the user profile — every launch rescans the bus.
 *
 * Only invoked once, right after MultipleDevices is constructed;
 * after startup, hotplug is handled by the existing PortMonitor
 * path (Windows WM_DEVICECHANGE, Linux libudev, Android JNI).
 */

#pragma once

#ifdef HAVE_REMOTE_STICK

#include <cstdint>
#include <optional>
#include <string>

namespace SteFly {

/**
 * The USB vendor ID assigned to SteFly on pid.codes.
 */
static constexpr std::uint16_t VID_STEFLY       = 0x1209;

/**
 * SteFly RemoteStick — joystick / key panel with rotary encoder.
 */
static constexpr std::uint16_t PID_REMOTE_STICK = 0x8500;

/**
 * SteFly RotaryPanel — encoder-only variant (scaffold, see
 * Device/Driver/SteFly/RotaryPanel.cpp).
 */
static constexpr std::uint16_t PID_ROTARY_PANEL = 0x8502;

/**
 * Scan the system for a USB CDC / ACM device matching the given
 * VID + PID and return its serial-port path.
 *
 * Implementations per platform:
 *   - Windows: SetupDiGetClassDevs(GUID_DEVCLASS_PORTS), match
 *     SPDRP_HARDWAREID against "USB\VID_xxxx&PID_xxxx", read
 *     "PortName" from the device parameters registry key.
 *   - Linux:   libudev enumerate "tty" subsystem, walk to the
 *     parent USB device, match idVendor / idProduct sysattrs,
 *     return devnode.
 *   - Android: not implemented (returns std::nullopt).
 *     UsbSerialHelper's own device list picks up VID/PID matches
 *     the normal way once the user hits the port picker.
 *
 * @return the port path (e.g. "COM7" or "/dev/ttyACM0"), or
 *         std::nullopt if no matching device is currently
 *         attached or the platform has no implementation.
 */
[[gnu::pure]]
std::optional<std::string>
DiscoverPortByUsbId(std::uint16_t vid, std::uint16_t pid) noexcept;

} // namespace SteFly

#endif // HAVE_REMOTE_STICK
