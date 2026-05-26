// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

// __linux__ is also set on Android (the kernel is Linux), but the NDK
// sysroot has no libudev — so require !__ANDROID__ as well.
#if defined(__linux__) && !defined(__ANDROID__) && defined(HAVE_LIBUDEV)

#include "event/PipeEvent.hxx"
#include "Operation/PopupOperationEnvironment.hpp"

class MultipleDevices;
struct udev;
struct udev_monitor;

/**
 * Linux USB / serial port hotplug detector.
 *
 * Wraps a libudev netlink monitor and forwards "add"/"remove" events
 * on the `tty` subsystem to MultipleDevices::DetectedPort / RemovedPort,
 * mirroring the Windows WM_DEVICECHANGE handler in
 * src/ui/window/gdi/Window.cpp.
 *
 * The monitor's file descriptor is integrated into the OpenSoar
 * EventLoop via PipeEvent, so events arrive without polling.
 */
class PortMonitorLinux final {
  ::udev *udev_handle = nullptr;
  ::udev_monitor *monitor = nullptr;
  PipeEvent fd_event;
  MultipleDevices &devices;
  PopupOperationEnvironment env;

public:
  PortMonitorLinux(EventLoop &event_loop, MultipleDevices &devices) noexcept;
  ~PortMonitorLinux() noexcept;

  PortMonitorLinux(const PortMonitorLinux &) = delete;
  PortMonitorLinux &operator=(const PortMonitorLinux &) = delete;

  /**
   * True if the udev monitor was set up successfully. If false the
   * object is harmless but a no-op (e.g. libudev unavailable, or
   * netlink not permitted in the current sandbox).
   */
  bool IsActive() const noexcept { return monitor != nullptr; }

private:
  void OnEvent(unsigned events) noexcept;
};

#endif // __linux__ && !__ANDROID__ && HAVE_LIBUDEV
