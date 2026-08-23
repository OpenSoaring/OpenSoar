// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Device/Config.hpp"

#include <string>
#include <vector>

/**
 * One entry of the Windows serial port enumeration, with everything
 * that was needed to classify it.  The raw registry strings are kept
 * so that a wrong classification can be diagnosed without guessing.
 */
struct DetectedSerialPort {
  /**
   * How the port should be opened.
   */
  DeviceConfig::PortType type = DeviceConfig::PortType::SERIAL;

  /**
   * The port name, e.g. "COM10".  This is what gets stored in the
   * device configuration.
   */
  std::string path;

  /**
   * The human-readable label, e.g. "COM10 (Larus)".
   */
  std::string display;

  /**
   * The device path from Hardware\DeviceMap\SerialComm, e.g.
   * "\Device\BthModem0".
   */
  std::string device_path;

  /**
   * The class string from the COM Name Arbiter, which tells Bluetooth,
   * USB, virtual and on-board ports apart.  Empty if it was not
   * readable.
   */
  std::string arbiter;

  /**
   * The address of the remote Bluetooth device, if this is a Bluetooth
   * port and the address could be parsed.
   */
  std::string address;

  /**
   * Shall this port be kept out of the port picker?
   */
  bool hidden = false;

  /**
   * Why the port is hidden; nullptr when it is not.  A static string,
   * meant for the log file and for diagnostics, not for the user.
   */
  const char *hidden_reason = nullptr;
};

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
