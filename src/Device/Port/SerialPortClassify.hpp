// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Device/Config.hpp"

#include <map>
#include <string>
#include <string_view>
#include <vector>

/**
 * One entry of the serial port enumeration, with everything that was
 * needed to classify it.  The raw strings are kept so that a wrong
 * classification can be diagnosed without guessing.
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
   * The device path the system reports, e.g. "\Device\BthModem0".
   */
  std::string device_path;

  /**
   * The class string which tells Bluetooth, USB, virtual and on-board
   * ports apart; on Windows this comes from the COM Name Arbiter.
   * Empty if it was not readable.
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
 * Maps a Bluetooth device address to the name the system knows for it.
 */
using BluetoothNameMap = std::map<std::string, std::string, std::less<>>;

/**
 * Classify one serial port from the raw strings the system provides.
 *
 * This is a pure function of its arguments, so it can be exercised
 * without any serial port being present - see TestSerialPorts.
 */
[[gnu::pure]]
DetectedSerialPort
ClassifySerialPort(std::string_view path, std::string_view device_path,
                   std::string_view arbiter,
                   const BluetoothNameMap &classic,
                   const BluetoothNameMap &le) noexcept;

/**
 * Decide whether the ports marked as hidden really stay hidden.  A
 * port is only dropped if at least one usable Bluetooth port remains,
 * so that this filter can never be the reason why the user has nothing
 * to pick.
 */
void
ResolveHiddenPorts(std::vector<DetectedSerialPort> &ports) noexcept;
