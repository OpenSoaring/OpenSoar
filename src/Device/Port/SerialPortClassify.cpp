// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "SerialPortClassify.hpp"
#include "util/IterableSplitString.hxx"

/**
 * Return the nth field of a string, or an empty view when there are
 * fewer fields.
 */
[[gnu::pure]]
static std::string_view
NthField(std::string_view s, char separator, unsigned n) noexcept
{
  for (const auto i : IterableSplitString(s, separator)) {
    if (n == 0)
      return i;
    --n;
  }

  return {};
}

/**
 * Extract the address of the remote device from a "bthenum" instance
 * path, which looks like
 *
 *   \\?\bthenum#{0000...}_localmfg&005d#9&2566f247&0&c049ef635562_c00000000#{...}
 *
 * The field before the trailing "_" is the address; "000000000000"
 * means there is no remote device, i.e. this is the local listener
 * which Windows creates alongside the outgoing port.
 */
[[gnu::pure]]
static std::string_view
ParseBluetoothAddress(std::string_view arbiter) noexcept
{
  /* the third "#" separated field carries address and direction */
  const auto instance = NthField(arbiter, '#', 2);
  if (instance.empty())
    return {};

  const auto before_direction = NthField(instance, '_', 0);
  if (before_direction.empty())
    return {};

  return NthField(before_direction, '&', 3);
}

static void
ClassifyBluetooth(DetectedSerialPort &port,
                  const BluetoothNameMap &classic,
                  const BluetoothNameMap &le) noexcept
{
  port.address = ParseBluetoothAddress(port.arbiter);

  if (port.address == "000000000000") {
    /* no remote device: the incoming port, waiting for the device to
       call us, which none of our devices does.  All traffic goes
       through the outgoing port. */
    port.kind = DetectedSerialPort::Kind::BLUETOOTH;
    port.display = port.path + " (incoming)";
    port.hidden = true;
    port.hidden_reason = "incoming Bluetooth port";
    return;
  }

  port.kind = DetectedSerialPort::Kind::BLUETOOTH;

  if (port.address.empty()) {
    /* an unparseable entry is still a usable RFCOMM port */
    port.display = port.path;
    return;
  }

  if (const auto i = le.find(port.address); i != le.end()) {
    port.kind = DetectedSerialPort::Kind::BLUETOOTH_LE;
    port.display = port.path + " (" + i->second + ")";
    return;
  }

  if (const auto i = classic.find(port.address); i != classic.end()) {
    port.display = port.path + " (" + i->second + ")";
    return;
  }

  /* a remote device we have no name for; show the address, which the
     user can look up in the Bluetooth settings */
  port.display = port.path + " (" + port.address + ")";
}

static void
ClassifyUsb(DetectedSerialPort &port) noexcept
{
  port.kind = DetectedSerialPort::Kind::USB;

  const auto driver = NthField(port.device_path, '\\', 2);
  if (!driver.starts_with("USBSER")) {
    port.display = driver.empty()
      ? port.path
      : port.path + " (" + std::string{driver} + ")";
    return;
  }

  /* the caller looks up the name the USB device reports; without it,
     say at least that it is a USB device */
  port.display = port.path + " (USB)";
}

DetectedSerialPort
ClassifySerialPort(std::string_view path, std::string_view device_path,
                   std::string_view arbiter,
                   const BluetoothNameMap &classic,
                   const BluetoothNameMap &le) noexcept
{
  DetectedSerialPort port;
  port.path = path;
  port.device_path = device_path;
  port.arbiter = arbiter;

  /* the class string tells the device kind apart:
     Bluetooth "\\?\bthenum#", USB "\\?\usb#",
     "\\?\root#" for virtual ports, "\\?\acpi#" for on-board ones */
  if (port.arbiter.starts_with("\\\\?\\bthenum#")) {
    ClassifyBluetooth(port, classic, le);
    return port;
  }

  if (port.arbiter.starts_with("\\\\?\\usb#")) {
    ClassifyUsb(port);
    return port;
  }

  port.kind = DetectedSerialPort::Kind::SERIAL;

  /* a virtual ("\\?\root#") or on-board ("\\?\acpi#") port: the
     driver's name of the port says which one it is, e.g. "COM1
     (Serial0)"; the port name always comes first, because that is the
     name OpenSoar stores and shows everywhere else */
  if (port.arbiter.empty())
    port.display = port.path;
  else if (const auto driver = NthField(port.device_path, '\\', 2);
           driver.empty())
    port.display = port.path;
  else
    port.display = port.path + " (" + std::string{driver} + ")";

  return port;
}

void
ResolveHiddenPorts(std::vector<DetectedSerialPort> &ports) noexcept
{
  for (const auto &port : ports)
    if (!port.hidden &&
        (port.kind == DetectedSerialPort::Kind::BLUETOOTH ||
         port.kind == DetectedSerialPort::Kind::BLUETOOTH_LE))
      /* at least one usable Bluetooth port remains, so the hidden
         ones stay hidden */
      return;

  for (auto &port : ports) {
    port.hidden = false;
    port.hidden_reason = nullptr;
  }
}
