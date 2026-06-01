// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef _WIN32

#include "PortMonitorWindows.hpp"
#include "MultipleDevices.hpp"
#include "LogFile.hpp"

#include <windows.h>
#include <Dbt.h>

PortMonitorWindows::PortMonitorWindows(MultipleDevices &_devices) noexcept
  :devices(_devices)
{
  LogString("PortMonitorWindows: armed");
}

PortMonitorWindows::~PortMonitorWindows() noexcept = default;

bool
PortMonitorWindows::HandleDeviceChange(WPARAM wParam, LPARAM lParam) noexcept
{
  if (lParam == 0)
    return false;

  auto *hdr = reinterpret_cast<PDEV_BROADCAST_HDR>(lParam);

  switch (hdr->dbch_devicetype) {

  case DBT_DEVTYP_DEVICEINTERFACE: {
    // Generic USB device interface events. The current contract
    // matches devices by COM name only, so we just log here — the
    // hook is ready if you later want to resolve VID:PID -> COM via
    // SetupAPI before forwarding to MultipleDevices.
    auto *p = reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(lParam);
    switch (wParam) {
    case DBT_DEVICEARRIVAL:
      LogFmt("PortMonitorWindows: device connected: {}", p->dbcc_name);
      break;
    case DBT_DEVICEREMOVECOMPLETE:
      LogFmt("PortMonitorWindows: device disconnected: {}", p->dbcc_name);
      break;
    }
    return true;
  }

  case DBT_DEVTYP_PORT: {
    // Legacy serial / USB-CDC COM-port notifications. dbcp_name is
    // the user-visible port name (e.g. "COM3") — exactly what the
    // OpenSoar device config stores.
    auto *p = reinterpret_cast<PDEV_BROADCAST_PORT>(lParam);
    switch (wParam) {
    case DBT_DEVICEARRIVAL:
      LogFmt("PortMonitorWindows: port connected: {}", p->dbcp_name);
      devices.DetectedPort(p->dbcp_name, env);
      break;
    case DBT_DEVICEREMOVECOMPLETE:
      LogFmt("PortMonitorWindows: port disconnected: {}", p->dbcp_name);
      devices.RemovedPort(p->dbcp_name, env);
      break;
    }
    return true;
  }

  default:
    LogFmt("PortMonitorWindows: WM_DEVICECHANGE other "
           "devicetype={}, wParam={}",
           hdr->dbch_devicetype, wParam);
    return false;
  }
}

#endif // _WIN32
