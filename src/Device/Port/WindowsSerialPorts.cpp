// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WindowsSerialPorts.hpp"
#include "system/WindowsRegistry.hpp"
#include "util/IterableSplitString.hxx"

#include <map>
#include <optional>
#include <span>

/**
 * Open a registry key.  Unlike the RegistryKey constructor, this does
 * not throw when the key is missing or unreadable - a key which was
 * never created (e.g. Enum\BthLE without a single paired Bluetooth LE
 * device) is a normal condition, not an error.
 */
static std::optional<RegistryKey>
TryOpenRegistryKey(HKEY parent, const char *key) noexcept
try {
  return RegistryKey{parent, key};
} catch (const std::system_error &) {
  return std::nullopt;
}

using NameMap = std::map<std::string, std::string>;

/**
 * Collect "device address" -> "friendly name" from one of the
 * Bluetooth enumeration keys.  Failures are per device: one unreadable
 * entry must not cost us the whole map.
 */
static NameMap
CollectBluetoothNames(const char *key_name, bool &available) noexcept
{
  NameMap map;

  const auto key = TryOpenRegistryKey(HKEY_LOCAL_MACHINE, key_name);
  available = key.has_value();
  if (!key)
    return map;

  for (unsigned k = 0;; ++k) {
    char dev_name[128], name[128], friendly_name[128];

    if (!key->EnumKey(k, std::span{dev_name}))
      break;

    std::string map_name{dev_name};
    if (!map_name.starts_with("Dev_"))
      continue;

    const auto dev = TryOpenRegistryKey(*key, dev_name);
    if (!dev || !dev->EnumKey(0, std::span{name}))
      continue;

    const auto sub = TryOpenRegistryKey(*dev, name);
    if (!sub || !sub->GetValue("FriendlyName", std::span{friendly_name}))
      continue;

    map_name = map_name.substr(4);
    for (auto &ch : map_name)
      if (ch >= 'A' && ch <= 'Z')
        ch += 'a' - 'A';

    map[map_name] = friendly_name;
  }

  return map;
}

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
static std::string
ParseBluetoothAddress(std::string_view arbiter) noexcept
{
  /* the third "#" separated field carries address and direction */
  const auto instance = NthField(arbiter, '#', 2);
  if (instance.empty())
    return {};

  const auto before_direction = NthField(instance, '_', 0);
  if (before_direction.empty())
    return {};

  return std::string{NthField(before_direction, '&', 3)};
}

static void
ClassifyBluetooth(DetectedSerialPort &port,
                  const NameMap &bthmap, const NameMap &blemap) noexcept
{
  port.address = ParseBluetoothAddress(port.arbiter);

  if (port.address == "000000000000") {
    /* no remote device: the incoming port, waiting for the device to
       call us, which none of our devices does.  All traffic goes
       through the outgoing port. */
    port.type = DeviceConfig::PortType::RFCOMM;
    port.display = port.path + " (incoming)";
    port.hidden = true;
    port.hidden_reason = "incoming Bluetooth port";
    return;
  }

  if (!port.address.empty()) {
    if (const auto ble = blemap.find(port.address); ble != blemap.end()) {
      port.type = DeviceConfig::PortType::BLE_HM10;
      port.display = port.path + " (" + ble->second + ")";
      return;
    }

    if (const auto bth = bthmap.find(port.address); bth != bthmap.end()) {
      port.type = DeviceConfig::PortType::RFCOMM;
      port.display = port.path + " (" + bth->second + ")";
      return;
    }

    /* a remote device we have no name for; show the address, which the
       user can look up in the Windows Bluetooth settings */
    port.type = DeviceConfig::PortType::RFCOMM;
    port.display = port.path + " (" + port.address + ")";
    return;
  }

  /* an unparseable entry is still a usable RFCOMM port */
  port.type = DeviceConfig::PortType::RFCOMM;
  port.display = port.path;
}

static void
ClassifyUsb(DetectedSerialPort &port) noexcept
{
  port.type = DeviceConfig::PortType::USB_SERIAL;

  const auto driver = NthField(port.device_path, '\\', 2);
  if (driver.starts_with("USBSER")) {
    /* look up the name the USB device reports */
    const auto vid_pid = NthField(port.arbiter, '#', 1);
    const auto instance = NthField(port.arbiter, '#', 2);

    if (!vid_pid.empty() && !instance.empty()) {
      std::string key{"SYSTEM\\CurrentControlSet\\ENUM\\USB\\"};
      key.append(vid_pid);   // e.g. "vid_1209&pid_8500&mi_00"
      key.push_back('\\');
      key.append(instance);  // e.g. "7&3226aff9&0&0000"

      char friendly_name[0x100];
      const auto usb = TryOpenRegistryKey(HKEY_LOCAL_MACHINE, key.c_str());
      if (usb && usb->GetValue("FriendlyName", std::span{friendly_name})) {
        port.display = friendly_name;
        return;
      }
    }

    port.display = port.path + " (USB)";
    return;
  }

  port.display = driver.empty()
    ? port.path
    : port.path + " (" + std::string{driver} + ")";
}

SerialPortEnumeration
EnumerateSerialPorts() noexcept
{
  SerialPortEnumeration result;

  /* the friendly names are optional; a missing Bluetooth key must not
     cost us the serial ports */
  const auto bthmap =
    CollectBluetoothNames("SYSTEM\\CurrentControlSet\\Enum\\BthEnum",
                          result.have_bthenum);
  const auto blemap =
    CollectBluetoothNames("SYSTEM\\CurrentControlSet\\Enum\\BthLE",
                          result.have_bthle);

  /* the registry key HKEY_LOCAL_MACHINE/Hardware/DEVICEMAP/SERIALCOMM
     is the best way to discover serial ports on Windows */
  const auto serialcomm =
    TryOpenRegistryKey(HKEY_LOCAL_MACHINE, "Hardware\\DeviceMap\\SerialComm");
  result.have_serialcomm = serialcomm.has_value();
  if (!serialcomm)
    return result;

  const auto com_devices =
    TryOpenRegistryKey(HKEY_LOCAL_MACHINE,
                       "SYSTEM\\CurrentControlSet\\Control\\"
                       "COM Name Arbiter\\Devices");
  result.have_arbiter = com_devices.has_value();

  unsigned n_bluetooth = 0;

  for (unsigned i = 0;; ++i) {
    char name[128], value[64];

    DWORD type;
    if (!serialcomm->EnumValue(i, std::span{name}, &type,
                               std::as_writable_bytes(std::span{value})))
      break;  // end of the list

    if (type != REG_SZ)
      // weird
      continue;

    DetectedSerialPort port;
    port.path = value;
    port.device_path = name;

    char arbiter[0x200];
    if (com_devices && com_devices->GetValue(value, std::span{arbiter}))
      port.arbiter = arbiter;

    /* the class string tells the device kind apart:
       Bluetooth "\\?\bthenum#", USB "\\?\usb#",
       "\\?\root#" for virtual ports, "\\?\acpi#" for on-board ones */
    if (port.arbiter.starts_with("\\\\?\\bthenum#")) {
      ClassifyBluetooth(port, bthmap, blemap);
      if (!port.hidden)
        ++n_bluetooth;
    } else if (port.arbiter.starts_with("\\\\?\\usb#")) {
      ClassifyUsb(port);
    } else if (port.arbiter.starts_with("\\\\?\\root#")) {
      const auto driver = NthField(port.device_path, '\\', 2);
      port.type = DeviceConfig::PortType::SERIAL;
      port.display = driver.empty()
        ? port.path
        : port.path + " (" + std::string{driver} + ")";
    } else {
      port.type = DeviceConfig::PortType::SERIAL;
      port.display = port.arbiter.empty() ? port.path : port.device_path;
    }

    result.ports.emplace_back(std::move(port));
  }

  if (n_bluetooth == 0)
    /* no usable Bluetooth port was found, so offer the doubtful ones
       after all - this filter must never leave the user without a
       port to pick */
    for (auto &port : result.ports)
      if (port.hidden) {
        port.hidden = false;
        port.hidden_reason = nullptr;
      }

  return result;
}
