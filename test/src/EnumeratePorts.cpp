// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef HAVE_POSIX
#include "Device/Port/TTYEnumerator.hpp"
#elif defined(_WIN32)
#include "system/WindowsRegistry.hpp"

#include <map>
#include <optional>
#include <span>
#include <string>
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32

/**
 * Open a registry key; report a missing key instead of throwing.
 */
static std::optional<RegistryKey>
TryOpenRegistryKey(HKEY parent, const char *key) noexcept
try {
  return RegistryKey{parent, key};
} catch (const std::system_error &) {
  return std::nullopt;
}

/**
 * Dump one of the Bluetooth enumeration keys: device address and the
 * friendly name Windows knows for it.
 */
static void
DumpBluetoothNames(const char *key_name) noexcept
{
  printf("\n[%s]\n", key_name);

  const auto key = TryOpenRegistryKey(HKEY_LOCAL_MACHINE, key_name);
  if (!key) {
    printf("  (key does not exist - no device of this kind was ever paired)\n");
    return;
  }

  unsigned n = 0;
  for (unsigned k = 0;; ++k) {
    char dev_name[128], name[128], friendly_name[128];

    if (!key->EnumKey(k, std::span{dev_name}))
      break;

    std::string map_name{dev_name};
    if (!map_name.starts_with("Dev_"))
      continue;

    const auto dev = TryOpenRegistryKey(*key, dev_name);
    if (!dev || !dev->EnumKey(0, std::span{name})) {
      printf("  %-16s (no subkey)\n", map_name.c_str() + 4);
      continue;
    }

    const auto sub = TryOpenRegistryKey(*dev, name);
    if (!sub || !sub->GetValue("FriendlyName", std::span{friendly_name})) {
      printf("  %-16s (no FriendlyName)\n", map_name.c_str() + 4);
      continue;
    }

    printf("  %-16s %s\n", map_name.c_str() + 4, friendly_name);
    ++n;
  }

  printf("  -> %u named device(s)\n", n);
}

/**
 * Dump the serial ports exactly as the registry offers them, without
 * interpreting anything: the COM name, the device path, and the class
 * string which tells Bluetooth, USB, virtual and on-board ports apart.
 */
static bool
DumpSerialPorts() noexcept
{
  printf("\n[Hardware\\DeviceMap\\SerialComm]\n");

  const auto serialcomm =
    TryOpenRegistryKey(HKEY_LOCAL_MACHINE, "Hardware\\DeviceMap\\SerialComm");
  if (!serialcomm) {
    printf("  (key does not exist - this machine has no serial port)\n");
    return false;
  }

  const auto com_devices =
    TryOpenRegistryKey(HKEY_LOCAL_MACHINE,
                       "SYSTEM\\CurrentControlSet\\Control\\"
                       "COM Name Arbiter\\Devices");
  if (!com_devices)
    printf("  (COM Name Arbiter not readable - no class information)\n");

  unsigned n = 0;
  for (unsigned i = 0;; ++i) {
    char name[128], value[64];

    DWORD type;
    if (!serialcomm->EnumValue(i, std::span{name}, &type,
                               std::as_writable_bytes(std::span{value})))
      break;

    if (type != REG_SZ)
      continue;

    char arbiter[0x200];
    const bool have_class =
      com_devices && com_devices->GetValue(value, std::span{arbiter});

    printf("  %-8s %-24s %s\n", value, name,
           have_class ? arbiter : "(no arbiter entry)");
    ++n;
  }

  printf("  -> %u port(s)\n", n);
  return n > 0;
}

#endif /* _WIN32 */

int main()
{
  bool implemented = false, success = false;

#ifdef HAVE_POSIX
  implemented = true;

  TTYEnumerator te;
  if (!te.HasFailed()) {
    success = true;

    const char *path;
    while ((path = te.Next()) != nullptr)
      printf("%s\n", path);
  } else
    fprintf(stderr, "Failed to enumerate TTY ports\n");
#elif defined(_WIN32)
  implemented = true;

  DumpBluetoothNames("SYSTEM\\CurrentControlSet\\Enum\\BthEnum");
  DumpBluetoothNames("SYSTEM\\CurrentControlSet\\Enum\\BthLE");
  success = DumpSerialPorts();
#endif

  if (!implemented)
    fprintf(stderr, "Port enumeration not implemented on this target\n");

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
