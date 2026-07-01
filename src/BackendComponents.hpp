// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <memory>

struct PolarSettings;
class Logger;
class NMEALogger;
class GlueFlightLogger;
class MultipleDevices;
class PortMonitor;
class PortMonitorLinux;
class PortMonitorWindows;
class DeviceBlackboard;
class MergeThread;
class ProtectedTaskManager;
class ProtectedAirspaceWarningManager;
class GlideComputer;
class CalculationThread;
class Replay;

/**
 * This singleton manages components that are part of XCSoar's backend
 * (e.g. sensors and other data sources, network clients, computers,
 * loggers).
 */
struct BackendComponents {
  std::unique_ptr<Logger> igc_logger;
  std::unique_ptr<NMEALogger> nmea_logger;
  std::unique_ptr<GlueFlightLogger> flight_logger;

  const std::unique_ptr<DeviceBlackboard> device_blackboard;
  std::unique_ptr<MultipleDevices> devices;
  /**
   * USB / serial hotplug monitor. Platform-specific implementation:
   *   Windows -> PortMonitorWindows (driven by WM_DEVICECHANGE)
   *   Linux   -> PortMonitorLinux   (libudev, optional)
   *   Android -> not used (the BroadcastReceiver in
   *              UsbSerialHelper.java calls into MultipleDevices
   *              directly via JNI)
   * Field is absent on platforms without an implementation, so a
   * `#ifdef` check is needed at every use site.
   */
#if defined(_WIN32)
  // Held via the abstract PortMonitor base so the screen library (which
  // dispatches WM_DEVICECHANGE through Window) needs no link-time
  // reference to the concrete PortMonitorWindows in main.
  std::unique_ptr<PortMonitor> port_monitor;
#elif defined(__linux__) && !defined(__ANDROID__) && defined(HAVE_LIBUDEV)
  std::unique_ptr<PortMonitorLinux> port_monitor;
#endif
  std::unique_ptr<MergeThread> merge_thread;

  std::unique_ptr<ProtectedTaskManager> protected_task_manager;
  std::unique_ptr<GlideComputer> glide_computer;
  std::unique_ptr<CalculationThread> calculation_thread;

  std::unique_ptr<Replay> replay;

  BackendComponents() noexcept;
  ~BackendComponents() noexcept;

  BackendComponents(const BackendComponents &) = delete;
  BackendComponents &operator=(const BackendComponents &) = delete;

/**
 * Returns the global ProtectedAirspaceWarningManager instance.  May
 * be nullptr if disabled.
 */
  [[gnu::pure]]
  ProtectedAirspaceWarningManager *GetAirspaceWarnings() noexcept;

  /**
   * Call this after modifying the #GlidePolar in #PolarSettings to
   * propagate it to the #CalculationThread and the
   * #ProtectedTaskManager.
   */
  void SetTaskPolar(const PolarSettings &settings) noexcept;
};
