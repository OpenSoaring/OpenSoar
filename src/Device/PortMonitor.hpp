// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#ifdef _WIN32
#include <windef.h>  // for WPARAM/LPARAM
#endif

/**
 * Abstract base class for the platform-specific USB / serial hotplug
 * monitors (PortMonitorWindows, PortMonitorLinux).
 *
 * BackendComponents holds the monitor through this base class so the
 * layout of BackendComponents is identical in every translation unit,
 * no matter which optional feature macros (e.g. HAVE_LIBUDEV) happen
 * to be defined there. A struct layout depending on a non-global
 * define is an ODR violation and has caused real memory corruption
 * once — never make members conditional on such macros.
 *
 * On platforms without an implementation the BackendComponents
 * pointer simply stays empty.
 */
class PortMonitor {
public:
  virtual ~PortMonitor() noexcept = default;

#ifdef _WIN32
  /**
   * Forward a WM_DEVICECHANGE message to the monitor.
   *
   * Declared in the base class so the generic GDI Window (screen
   * library) can dispatch through this interface without a link-time
   * reference to the concrete PortMonitorWindows in main. _WIN32 is a
   * compiler-predefined macro and therefore consistent across all
   * translation units of a build — unlike HAVE_LIBUDEV.
   *
   * @param wParam  event subtype (DBT_DEVICEARRIVAL / DBT_DEVICEREMOVECOMPLETE / …)
   * @param lParam  pointer to a DEV_BROADCAST_HDR or NULL
   * @return true if the monitor recognised the event (informational)
   */
  virtual bool HandleDeviceChange(WPARAM wParam, LPARAM lParam) noexcept = 0;
#endif
};
