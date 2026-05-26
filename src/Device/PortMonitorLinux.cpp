// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

// __linux__ is also set on Android, but the NDK sysroot has no libudev,
// so guard against that too.
#if defined(__linux__) && !defined(__ANDROID__) && defined(HAVE_LIBUDEV)

#include "PortMonitorLinux.hpp"
#include "MultipleDevices.hpp"
#include "LogFile.hpp"
#include "io/FileDescriptor.hxx"

#include <libudev.h>

#include <string_view>

PortMonitorLinux::PortMonitorLinux(EventLoop &event_loop,
                                   MultipleDevices &_devices) noexcept
  :fd_event(event_loop, BIND_THIS_METHOD(OnEvent)),
   devices(_devices)
{
  udev_handle = udev_new();
  if (udev_handle == nullptr) {
    LogString("PortMonitorLinux: udev_new() failed");
    return;
  }

  monitor = udev_monitor_new_from_netlink(udev_handle, "udev");
  if (monitor == nullptr) {
    LogString("PortMonitorLinux: udev_monitor_new_from_netlink() failed");
    udev_unref(udev_handle);
    udev_handle = nullptr;
    return;
  }

  // We're interested in serial ports / USB-CDC ACM devices that appear
  // under /dev/tty* — the same kind of paths a device config holds.
  udev_monitor_filter_add_match_subsystem_devtype(monitor, "tty", nullptr);

  if (udev_monitor_enable_receiving(monitor) < 0) {
    LogString("PortMonitorLinux: udev_monitor_enable_receiving() failed");
    udev_monitor_unref(monitor);
    monitor = nullptr;
    udev_unref(udev_handle);
    udev_handle = nullptr;
    return;
  }

  const int fd = udev_monitor_get_fd(monitor);
  if (fd < 0) {
    LogString("PortMonitorLinux: udev_monitor_get_fd() failed");
    udev_monitor_unref(monitor);
    monitor = nullptr;
    udev_unref(udev_handle);
    udev_handle = nullptr;
    return;
  }

  fd_event.Open(FileDescriptor{fd});
  fd_event.ScheduleRead();
  LogString("PortMonitorLinux: udev hotplug monitor active");
}

PortMonitorLinux::~PortMonitorLinux() noexcept
{
  // PipeEvent does NOT close the FD — that's owned by udev_monitor.
  // Cancel polling first, then release the FD without closing it.
  fd_event.Cancel();
  if (fd_event.IsDefined())
    fd_event.ReleaseFileDescriptor();

  if (monitor != nullptr)
    udev_monitor_unref(monitor);
  if (udev_handle != nullptr)
    udev_unref(udev_handle);
}

void
PortMonitorLinux::OnEvent([[maybe_unused]] unsigned events) noexcept
{
  if (monitor == nullptr)
    return;

  // Drain all currently-available events. The FD is edge-triggered in
  // some kernels, so loop until receive_device() returns nullptr.
  while (auto *dev = udev_monitor_receive_device(monitor)) {
    const char *action = udev_device_get_action(dev);
    const char *devnode = udev_device_get_devnode(dev);

    if (action != nullptr && devnode != nullptr) {
      const std::string_view portname{devnode};
      const std::string_view action_view{action};
      if (action_view == "add") {
        LogFmt("PortMonitorLinux: device added: {}", devnode);
        devices.DetectedPort(portname, env);
      } else if (action_view == "remove") {
        LogFmt("PortMonitorLinux: device removed: {}", devnode);
        devices.RemovedPort(portname, env);
      }
    }

    udev_device_unref(dev);
  }

  fd_event.ScheduleRead();
}

#endif // __linux__ && !__ANDROID__ && HAVE_LIBUDEV
