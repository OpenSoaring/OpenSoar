// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef HAVE_POSIX
#include "Device/Port/TTYEnumerator.hpp"
#elif defined(_WIN32)
#include "Device/Port/WindowsSerialPorts.hpp"
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32

static const char *
ToString(DeviceConfig::PortType type) noexcept
{
  switch (type) {
  case DeviceConfig::PortType::SERIAL:
    return "serial";

  case DeviceConfig::PortType::RFCOMM:
    return "bluetooth";

  case DeviceConfig::PortType::BLE_HM10:
    return "bluetooth-le";

  case DeviceConfig::PortType::USB_SERIAL:
    return "usb";

  default:
    return "other";
  }
}

static bool
DumpSerialPorts() noexcept
{
  const auto e = EnumerateSerialPorts();

  printf("registry keys:\n");
  printf("  Enum\\BthEnum            %s\n",
         e.have_bthenum ? "yes" : "MISSING (no classic device ever paired)");
  printf("  Enum\\BthLE              %s\n",
         e.have_bthle ? "yes" : "MISSING (no LE device ever paired)");
  printf("  DeviceMap\\SerialComm    %s\n",
         e.have_serialcomm ? "yes" : "MISSING (no serial port at all)");
  printf("  COM Name Arbiter        %s\n",
         e.have_arbiter ? "yes" : "MISSING (device classes unknown)");

  printf("\n%zu port(s):\n", e.ports.size());

  for (const auto &port : e.ports) {
    printf("\n  %s\n", port.path.c_str());
    printf("    shown as   %s\n", port.display.c_str());
    printf("    type       %s\n", ToString(port.type));
    printf("    device     %s\n", port.device_path.c_str());
    printf("    class      %s\n",
           port.arbiter.empty() ? "(no arbiter entry)" : port.arbiter.c_str());

    if (!port.address.empty())
      printf("    address    %s\n", port.address.c_str());

    if (port.hidden)
      printf("    HIDDEN     %s\n", port.hidden_reason);
  }

  return !e.ports.empty();
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
  success = DumpSerialPorts();
#endif

  if (!implemented)
    fprintf(stderr, "Port enumeration not implemented on this target\n");

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
