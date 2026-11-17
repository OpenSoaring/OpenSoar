// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <string>
#include <string_view>

/**
 * The number of a Windows serial port, whatever spelling the name
 * has: "COM3", "COM3:" or "\\.\COM3".
 *
 * @return 0 if the name is not a COM port
 */
[[gnu::pure]]
unsigned
ParseComPortNumber(std::string_view name) noexcept;

/**
 * Read one message of the Linux kernel about a device (a "uevent", as
 * it arrives on a NETLINK_KOBJECT_UEVENT socket) and tell whether a
 * serial port has appeared or gone.
 *
 * @param added set to true if the port has appeared, to false if it
 * is gone
 * @param device set to the device node, e.g. "/dev/ttyUSB0"
 * @return false if the message is about something else
 */
bool
ParseTtyUevent(std::string_view message, bool &added,
               std::string &device) noexcept;
