// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef HAVE_REMOTE_STICK

#include "Discovery.hpp"
#include "LogFile.hpp"

#include <cstdio>
#include <cstring>

// -----------------------------------------------------------------------
// Windows: SetupAPI enumeration of the "Ports (COM & LPT)" device class.
// -----------------------------------------------------------------------
#if defined(_WIN32)

#include <windows.h>
#include <setupapi.h>
#include <devguid.h>

namespace SteFly {

std::optional<std::string>
DiscoverPortByUsbId(std::uint16_t vid, std::uint16_t pid) noexcept
{
  HDEVINFO hdi = SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS,
                                      /*Enumerator=*/ nullptr,
                                      /*hwndParent=*/ nullptr,
                                      DIGCF_PRESENT);
  if (hdi == INVALID_HANDLE_VALUE) {
    LogFmt("SteFly::Discovery: SetupDiGetClassDevs failed ({})",
           (unsigned)GetLastError());
    return std::nullopt;
  }

  // "USB\VID_1209&PID_8500" — first substring we look for in every
  // Hardware-ID line. Length capped at 24 to keep the buffer small.
  char wanted[32];
  std::snprintf(wanted, sizeof(wanted), "VID_%04X&PID_%04X", vid, pid);

  std::optional<std::string> result;

  SP_DEVINFO_DATA dev{};
  dev.cbSize = sizeof(dev);

  for (DWORD i = 0; SetupDiEnumDeviceInfo(hdi, i, &dev); ++i) {
    // SPDRP_HARDWAREID is a REG_MULTI_SZ (double-null-terminated list
    // of strings). We only need to spot the VID/PID substring anywhere
    // in the whole blob, so a plain strstr on the flat buffer works —
    // the inner NULs terminate individual strings but the substring
    // never crosses one.
    char hwid[512] = {};
    if (!SetupDiGetDeviceRegistryPropertyA(hdi, &dev, SPDRP_HARDWAREID,
                                           /*PropertyRegDataType=*/ nullptr,
                                           reinterpret_cast<PBYTE>(hwid),
                                           sizeof(hwid) - 1,
                                           /*RequiredSize=*/ nullptr))
      continue;

    if (std::strstr(hwid, wanted) == nullptr)
      continue;

    // Match — pull the "PortName" value ("COM7") from the device
    // parameters key. This is exactly what Device Manager displays as
    // the user-facing port name and what CreateFile() expects (with a
    // "\\.\" prefix for COM10+).
    HKEY hk = SetupDiOpenDevRegKey(hdi, &dev, DICS_FLAG_GLOBAL, 0,
                                   DIREG_DEV, KEY_READ);
    if (hk == INVALID_HANDLE_VALUE)
      continue;

    char port[32] = {};
    DWORD port_sz = sizeof(port);
    LONG rc = RegQueryValueExA(hk, "PortName", nullptr, nullptr,
                               reinterpret_cast<LPBYTE>(port), &port_sz);
    RegCloseKey(hk);

    if (rc != ERROR_SUCCESS)
      continue;

    LogFmt("SteFly::Discovery: matched VID_{:04X}&PID_{:04X} on port {}",
           vid, pid, port);
    result.emplace(port);
    break;
  }

  SetupDiDestroyDeviceInfoList(hdi);
  return result;
}

} // namespace SteFly

// -----------------------------------------------------------------------
// Linux (non-Android): libudev enumeration of the "tty" subsystem.
// Enabled only when the CMake / Makefile probe found libudev — same
// switch that gates PortMonitorLinux.
// -----------------------------------------------------------------------
#elif defined(__linux__) && !defined(__ANDROID__) && defined(HAVE_LIBUDEV)

#include <libudev.h>

namespace SteFly {

std::optional<std::string>
DiscoverPortByUsbId(std::uint16_t vid, std::uint16_t pid) noexcept
{
  udev *ud = udev_new();
  if (ud == nullptr)
    return std::nullopt;

  udev_enumerate *en = udev_enumerate_new(ud);
  if (en == nullptr) {
    udev_unref(ud);
    return std::nullopt;
  }

  udev_enumerate_add_match_subsystem(en, "tty");
  udev_enumerate_scan_devices(en);

  char want_vid[8], want_pid[8];
  // sysfs stores idVendor / idProduct as lower-case 4-digit hex.
  std::snprintf(want_vid, sizeof(want_vid), "%04x", vid);
  std::snprintf(want_pid, sizeof(want_pid), "%04x", pid);

  std::optional<std::string> result;

  for (udev_list_entry *le = udev_enumerate_get_list_entry(en);
       le != nullptr && !result;
       le = udev_list_entry_get_next(le)) {
    const char *syspath = udev_list_entry_get_name(le);
    udev_device *dev = udev_device_new_from_syspath(ud, syspath);
    if (dev == nullptr)
      continue;

    // Walk up the sysfs tree to the enclosing USB device — that's
    // where idVendor / idProduct live. Non-USB serial ports (built-in
    // 16550, PL2303 on a hub adapter, …) skip this step entirely.
    udev_device *usb = udev_device_get_parent_with_subsystem_devtype(
        dev, "usb", "usb_device");
    if (usb != nullptr) {
      const char *v = udev_device_get_sysattr_value(usb, "idVendor");
      const char *p = udev_device_get_sysattr_value(usb, "idProduct");
      if (v != nullptr && p != nullptr &&
          std::strcmp(v, want_vid) == 0 &&
          std::strcmp(p, want_pid) == 0) {
        const char *node = udev_device_get_devnode(dev);
        if (node != nullptr) {
          LogFmt("SteFly::Discovery: matched {}:{} on {}", want_vid, want_pid,
                 node);
          result.emplace(node);
        }
      }
    }

    udev_device_unref(dev);
  }

  udev_enumerate_unref(en);
  udev_unref(ud);
  return result;
}

} // namespace SteFly

// -----------------------------------------------------------------------
// Fallback (Android, macOS, plain Linux w/o libudev, …): no auto-scan.
// The user can still pick the RemoteStick manually until we grow a
// platform-specific implementation here.
// -----------------------------------------------------------------------
#else

namespace SteFly {

std::optional<std::string>
DiscoverPortByUsbId(std::uint16_t /*vid*/, std::uint16_t /*pid*/) noexcept
{
  return std::nullopt;
}

} // namespace SteFly

#endif

#endif // HAVE_REMOTE_STICK
