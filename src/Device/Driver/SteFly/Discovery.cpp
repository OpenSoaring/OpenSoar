// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef HAVE_REMOTE_STICK

#include "Discovery.hpp"
#include "LogFile.hpp"

#include <cstdio>
#include <cstring>

#if defined(__linux__)
#include <climits>
#include <cstdlib>

namespace SteFly {

/**
 * Read one 4-digit hex id ("idVendor" / "idProduct") from a sysfs
 * USB device directory.
 */
static bool
ReadSysfsHex(const std::string &dir, const char *attribute,
             std::uint16_t &value) noexcept
{
  const std::string path = dir + "/" + attribute;
  FILE *file = std::fopen(path.c_str(), "r");
  if (file == nullptr)
    return false;

  unsigned v;
  const bool ok = std::fscanf(file, "%x", &v) == 1 && v <= 0xffff;
  std::fclose(file);
  if (ok)
    value = static_cast<std::uint16_t>(v);
  return ok;
}

/**
 * Find the USB vendor and product id of the device behind the tty
 * with the given kernel name (e.g. "ttyACM0").
 *
 * /sys/class/tty/<name>/device points to the USB interface for a
 * CDC-ACM node (ttyACM*), and to a port directory one level below the
 * interface for a usb-serial node (ttyUSB*). The USB device that owns
 * idVendor / idProduct is the next ancestor that has them, so a few
 * levels up are searched; stopping at the first hit avoids picking up
 * the ids of the hub the device is connected to. Ttys without a
 * "device" link (virtual consoles, ptys) and on-board UARTs fail here
 * and are simply not USB devices.
 */
[[maybe_unused]]
static bool
ReadTtyUsbId(const char *name, std::uint16_t &vid,
             std::uint16_t &pid) noexcept
{
  const std::string link = std::string("/sys/class/tty/") + name + "/device";
  char real[PATH_MAX];
  if (realpath(link.c_str(), real) == nullptr)
    return false;

  std::string dir = real;
  for (unsigned level = 0; level < 4; ++level) {
    if (ReadSysfsHex(dir, "idVendor", vid) &&
        ReadSysfsHex(dir, "idProduct", pid))
      return true;

    const auto slash = dir.rfind('/');
    if (slash == std::string::npos || slash == 0)
      break;
    dir.erase(slash);
  }

  return false;
}

} // namespace SteFly
#endif

// -----------------------------------------------------------------------
// Windows: SetupAPI enumeration of the "Ports (COM & LPT)" device class.
// -----------------------------------------------------------------------
#if defined(_WIN32)

#include <windows.h>
#include <setupapi.h>
#include <devguid.h>

namespace SteFly {

std::optional<DiscoveredPort>
DiscoverPortByUsbId(std::uint16_t vid, std::uint16_t pid) noexcept
{
  /*
  HDEVINFO hdi = SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS,
                                      nullptr,  // Enumerator
                                      nullptr,  // hwndParent
                                      DIGCF_PRESENT);
/*/
HDEVINFO hdi = INVALID_HANDLE_VALUE;
/**/
if (hdi == INVALID_HANDLE_VALUE) {
    LogFmt("SteFly::Discovery: SetupDiGetClassDevs failed ({})",
           (unsigned)GetLastError());
    return std::nullopt;
  }

  // "USB\VID_1209&PID_8500" — first substring we look for in every
  // Hardware-ID line. Length capped at 24 to keep the buffer small.
  char wanted[32];
  std::snprintf(wanted, sizeof(wanted), "VID_%04X&PID_%04X", vid, pid);

  std::optional<DiscoveredPort> result;

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
    result.emplace(DiscoveredPort{DeviceConfig::PortType::SERIAL, port});
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

std::optional<DiscoveredPort>
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

  std::optional<DiscoveredPort> result;

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
          result.emplace(DiscoveredPort{DeviceConfig::PortType::SERIAL,
                                        node});
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
// Linux (non-Android) without libudev: walk /sys/class/tty directly.
// OpenVario, Kobo and other minimal images are built without libudev,
// but every Linux kernel exposes the same USB ids in sysfs, so the
// RemoteStick is found there just as well - only without the hotplug
// monitor, which is not needed for the startup scan.
// -----------------------------------------------------------------------
#elif defined(__linux__) && !defined(__ANDROID__)

#include <dirent.h>

namespace SteFly {

std::optional<DiscoveredPort>
DiscoverPortByUsbId(std::uint16_t vid, std::uint16_t pid) noexcept
{
  DIR *dir = opendir("/sys/class/tty");
  if (dir == nullptr) {
    LogFmt("SteFly::Discovery: /sys/class/tty is not readable");
    return std::nullopt;
  }

  std::optional<DiscoveredPort> result;

  while (const struct dirent *ent = readdir(dir)) {
    if (ent->d_name[0] == '.')
      continue;

    std::uint16_t v, p;
    if (!ReadTtyUsbId(ent->d_name, v, p) || v != vid || p != pid)
      continue;

    std::string node = "/dev/";
    node += ent->d_name;
    LogFmt("SteFly::Discovery: matched {:04x}:{:04x} on {}", vid, pid,
           node.c_str());
    result.emplace(DiscoveredPort{DeviceConfig::PortType::SERIAL,
                                  std::move(node)});
    break;
  }

  closedir(dir);
  return result;
}

} // namespace SteFly

// -----------------------------------------------------------------------
// Android: the app has no access to the tty device nodes, USB serial
// devices are opened through the Java UsbSerialHelper. Its constructor
// (called from Android/Main.cpp before Startup() runs) has already
// enumerated UsbManager.getDeviceList(), so the answer is available
// synchronously here. The USB permission is not needed for the lookup;
// opening the port asks for it if the user has not granted it yet.
// -----------------------------------------------------------------------
#elif defined(__ANDROID__)

#include "Android/Main.hpp"
#include "Android/UsbSerialHelper.hpp"
#include "java/Global.hxx"

namespace SteFly {

std::optional<DiscoveredPort>
DiscoverPortByUsbId(std::uint16_t vid, std::uint16_t pid) noexcept
{
  if (usb_serial_helper == nullptr) {
    LogFmt("SteFly::Discovery: no USB host support, skipping the scan");
    return std::nullopt;
  }

  auto id = usb_serial_helper->FindPortId(Java::GetEnv(), vid, pid);
  if (!id)
    return std::nullopt;

  LogFmt("SteFly::Discovery: matched {:04X}:{:04X} as USB port {}", vid, pid,
         id->c_str());
  return DiscoveredPort{DeviceConfig::PortType::ANDROID_USB_SERIAL, std::move(*id)};
}

} // namespace SteFly

// -----------------------------------------------------------------------
// Fallback (macOS, …): no auto-scan.
// -----------------------------------------------------------------------
#else

namespace SteFly {

std::optional<DiscoveredPort>
DiscoverPortByUsbId(std::uint16_t /*vid*/, std::uint16_t /*pid*/) noexcept
{
  return std::nullopt;
}

} // namespace SteFly

#endif

// -----------------------------------------------------------------------
// IsUsbTty(): Linux and Android share the sysfs layout.
// -----------------------------------------------------------------------
#if defined(__linux__)

namespace SteFly {

bool
IsUsbTty(const char *path, std::uint16_t vid, std::uint16_t pid) noexcept
{
  if (path == nullptr)
    return false;

  /* resolve udev symlinks such as /dev/serial/by-id/..., because
     sysfs only knows the kernel name of the node */
  char real[PATH_MAX];
  if (realpath(path, real) == nullptr)
    return false;

  const char *name = std::strrchr(real, '/');
  name = name != nullptr ? name + 1 : real;

  std::uint16_t v, p;
  return ReadTtyUsbId(name, v, p) && v == vid && p == pid;
}

} // namespace SteFly

#else

namespace SteFly {

bool
IsUsbTty(const char * /*path*/, std::uint16_t /*vid*/,
         std::uint16_t /*pid*/) noexcept
{
  return false;
}

} // namespace SteFly

#endif

#endif // HAVE_REMOTE_STICK
