// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

/** \file
 *
 * Startup-time USB / serial discovery for the SteFly device family
 * (RemoteStick, RotaryPanel).
 *
 * The result is one port (e.g. "COM7" on Windows, "/dev/ttyACM0" on
 * Linux or the UsbSerialHelper id "1209:8500#1" on Android) which
 * Startup.cpp uses to pre-configure the fixed REMOTE_PORT slot in
 * SystemSettings::devices before devStartup() opens the ports. This
 * slot is never persisted to the user profile - every launch rescans
 * the bus.
 *
 * Only invoked once, right after MultipleDevices is constructed. A
 * RemoteStick that is plugged in after startup is deliberately not
 * bound: most installations (smartphones, tablets without a stick)
 * never see one, and they should not pay for a permanent watch on
 * the bus. The existing PortMonitor path (Windows WM_DEVICECHANGE,
 * Linux libudev, Android JNI) still reopens the slot when a stick
 * that was found at startup is unplugged and plugged in again.
 */

#pragma once

#ifdef HAVE_REMOTE_STICK

#include "Device/Config.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace SteFly {

/**
 * The USB vendor ID assigned to SteFly on pid.codes.
 */
static constexpr std::uint16_t VID_STEFLY       = 0x1209;

/**
 * SteFly RemoteStick - joystick / key panel with rotary encoder.
 */
static constexpr std::uint16_t PID_REMOTE_STICK = 0x8500;

/**
 * SteFly RotaryPanel - encoder-only variant (scaffold, see
 * Device/Driver/SteFly/RotaryPanel.cpp).
 */
static constexpr std::uint16_t PID_ROTARY_PANEL = 0x8502;

/**
 * A port found by DiscoverPortByUsbId(). The port type is part of the
 * result because it differs between platforms: Windows and Linux open
 * a serial device node, while Android has no access to device nodes
 * and has to go through the Java UsbSerialHelper instead.
 */
struct DiscoveredPort {
  DeviceConfig::PortType type;
  std::string path;
};

/**
 * Scan the system for a USB CDC / ACM device matching the given
 * VID + PID and return the port that opens it.
 *
 * Implementations per platform:
 *   - Windows: SetupDiGetClassDevs(GUID_DEVCLASS_PORTS), match
 *     SPDRP_HARDWAREID against "USB\VID_xxxx&PID_xxxx", read
 *     "PortName" from the device parameters registry key.
 *   - Linux with libudev: enumerate the "tty" subsystem, walk to the
 *     parent USB device, match idVendor / idProduct, return devnode.
 *   - Linux without libudev (OpenVario, Kobo, minimal images): the
 *     same walk directly through /sys/class/tty.
 *   - Android: ask the Java UsbSerialHelper, which has already
 *     enumerated UsbManager.getDeviceList() when it was constructed.
 *
 * @return the port, or std::nullopt if no matching device is
 *         currently attached or the platform has no implementation
 */
std::optional<DiscoveredPort>
DiscoverPortByUsbId(std::uint16_t vid, std::uint16_t pid) noexcept;

/**
 * Does the given serial device node (e.g. "/dev/ttyUSB0") belong to
 * a USB device with this VID + PID?
 *
 * The port picker uses this to hide the kernel's own tty for a
 * RemoteStick whose slot is opened through a different path - on
 * Android the slot uses the UsbSerialHelper id, but some devices
 * additionally expose the stick as /dev/ttyUSB* or /dev/ttyACM*, and
 * a regular slot opened on that node would race the RemoteStick
 * descriptor for the same data.
 *
 * Only implemented on Linux and Android (via sysfs); returns false
 * elsewhere, and also when sysfs is not readable.
 */
bool
IsUsbTty(const char *path, std::uint16_t vid, std::uint16_t pid) noexcept;

} // namespace SteFly

#endif // HAVE_REMOTE_STICK
