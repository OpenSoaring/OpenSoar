// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "WindowsSerialPorts.hpp"
#include "system/WindowsRegistry.hpp"

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

/**
 * Collect "device address" -> "friendly name" from one of the
 * Bluetooth enumeration keys.  Failures are per device: one unreadable
 * entry must not cost us the whole map.
 */
static BluetoothNameMap
CollectBluetoothNames(const char *key_name, bool &available) noexcept
{
  BluetoothNameMap map;

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
 * A USB serial adapter usually reports a name of its own; look it up
 * and use it instead of the generic label.
 */
static void
LookUpUsbName(DetectedSerialPort &port) noexcept
{
  /* the vendor/product field and the instance field of
     \\?\usb#vid_1209&pid_8500&mi_00#7&3226aff9&0&0000#{...} */
  const auto hash1 = port.arbiter.find('#');
  if (hash1 == std::string::npos)
    return;

  const auto hash2 = port.arbiter.find('#', hash1 + 1);
  if (hash2 == std::string::npos)
    return;

  const auto hash3 = port.arbiter.find('#', hash2 + 1);
  if (hash3 == std::string::npos)
    return;

  std::string key{"SYSTEM\\CurrentControlSet\\ENUM\\USB\\"};
  key.append(port.arbiter, hash1 + 1, hash2 - hash1 - 1);
  key.push_back('\\');
  key.append(port.arbiter, hash2 + 1, hash3 - hash2 - 1);

  char friendly_name[0x100];
  const auto usb = TryOpenRegistryKey(HKEY_LOCAL_MACHINE, key.c_str());
  if (usb && usb->GetValue("FriendlyName", std::span{friendly_name}))
    port.display = friendly_name;
}

SerialPortEnumeration
EnumerateSerialPorts() noexcept
{
  SerialPortEnumeration result;

  /* the friendly names are optional; a missing Bluetooth key must not
     cost us the serial ports */
  const auto classic =
    CollectBluetoothNames("SYSTEM\\CurrentControlSet\\Enum\\BthEnum",
                          result.have_bthenum);
  const auto le =
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

  for (unsigned i = 0;; ++i) {
    char name[128], value[64];

    DWORD type;
    if (!serialcomm->EnumValue(i, std::span{name}, &type,
                               std::as_writable_bytes(std::span{value})))
      break;  // end of the list

    if (type != REG_SZ)
      // weird
      continue;

    char arbiter[0x200];
    const bool have_class =
      com_devices && com_devices->GetValue(value, std::span{arbiter});

    auto port = ClassifySerialPort(value, name,
                                   have_class ? arbiter : "",
                                   classic, le);

    if (port.type == DeviceConfig::PortType::USB_SERIAL &&
        port.display.ends_with("(USB)"))
      /* a USBSER device, for which the classifier could only say
         "USB"; the registry usually knows a better name */
      LookUpUsbName(port);

    result.ports.emplace_back(std::move(port));
  }

  ResolveHiddenPorts(result.ports);

  return result;
}
