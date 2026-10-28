// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "SerialPortClassify.hpp"

/**
 * The result of the enumeration, including which registry keys were
 * available.  A missing key is not an error: Enum\BthLE for instance
 * only exists once a Bluetooth LE device has been paired.
 */
struct SerialPortEnumeration {
  std::vector<DetectedSerialPort> ports;

  bool have_bthenum = false;
  bool have_bthle = false;
  bool have_serialcomm = false;
  bool have_arbiter = false;
};

/**
 * Enumerate the serial ports of this machine and classify them.
 *
 * Ports which are of no use are marked as hidden rather than dropped,
 * and a port is never hidden if that would leave no Bluetooth port at
 * all - so this function can never be the reason why the user has
 * nothing to pick.
 */
SerialPortEnumeration
EnumerateSerialPorts() noexcept;
