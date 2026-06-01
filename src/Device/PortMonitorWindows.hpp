// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#ifdef _WIN32

#include "Operation/PopupOperationEnvironment.hpp"

#include <windef.h>  // for WPARAM/LPARAM

class MultipleDevices;

/**
 * Windows USB / serial port hotplug detector.
 *
 * Receives WM_DEVICECHANGE notifications via HandleDeviceChange()
 * (forwarded from the main window's message pump) and dispatches
 * the resulting port name to MultipleDevices::DetectedPort and
 * MultipleDevices::RemovedPort — mirroring PortMonitorLinux
 * (libudev) and the Android JNI bridge in UsbSerialHelper.cpp.
 *
 * The monitor itself is passive: it owns no window or HDEVNOTIFY
 * handle. The existing main window already receives WM_DEVICECHANGE
 * for free for DBT_DEVTYP_PORT (legacy COM port notifications).
 * If you ever want DBT_DEVTYP_DEVICEINTERFACE events too, register
 * a HDEVNOTIFY in the constructor with RegisterDeviceNotification()
 * — but for the current "match by COM name" flow that is not needed.
 */
class PortMonitorWindows final {
  MultipleDevices &devices;
  PopupOperationEnvironment env;

public:
  explicit PortMonitorWindows(MultipleDevices &devices) noexcept;
  ~PortMonitorWindows() noexcept;

  PortMonitorWindows(const PortMonitorWindows &) = delete;
  PortMonitorWindows &operator=(const PortMonitorWindows &) = delete;

  /**
   * Forward a WM_DEVICECHANGE message to the monitor.
   *
   * @param wParam  event subtype (DBT_DEVICEARRIVAL / DBT_DEVICEREMOVECOMPLETE / …)
   * @param lParam  pointer to a DEV_BROADCAST_HDR or NULL
   * @return true if the monitor recognised the event (informational)
   */
  bool HandleDeviceChange(WPARAM wParam, LPARAM lParam) noexcept;
};

#endif // _WIN32
