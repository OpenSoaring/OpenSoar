// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Calibrate.hpp"
#include "Dialogs/Error.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/ProcessDialog.hpp"
#include "Form/Form.hpp"
#include "UIGlobals.hpp"
#include "lib/dbus/Connection.hxx"
#include "lib/dbus/ScopeMatch.hxx"
#include "lib/dbus/Systemd.hxx"
#include "system/Process.hpp"
#include "ui/display/Display.hpp"
#include "ui/event/Queue.hpp"
#include "util/ScopeExit.hxx"

#include <cstdlib>
#include <stdexcept>

void
CalibrateSensors() noexcept
try {
  /* make sure sensord is stopped while calibrating sensors */
  auto connection = ODBus::Connection::GetSystem();
  const ODBus::ScopeMatch job_removed_match{connection, Systemd::job_removed_match};

  bool has_sensord = false;

  if (Systemd::IsUnitActive(connection, "sensord.socket")) {
    has_sensord = true;

    try {
      Systemd::StopUnit(connection, "sensord.socket");
    } catch (...) {
      std::throw_with_nested(std::runtime_error{"Failed to stop sensord"});
    }
  }

  AtScopeExit(&connection, has_sensord){
    if (has_sensord)
      Systemd::StartUnit(connection, "sensord.socket");
  };

  /* calibrate the sensors */
  static constexpr const char *calibrate_sensors[] = {
    "/opt/bin/sensorcal", "-c", nullptr
  };

  static constexpr int STATUS_BOARD_NOT_INITIALISED = 2;
  static constexpr int RESULT_BOARD_NOT_INITIALISED = 100;
  int result = RunProcessDialog(UIGlobals::GetMainWindow(),
                                UIGlobals::GetDialogLook(),
                                "Calibrate Sensors", calibrate_sensors,
                                [](int status){
                                  return status == STATUS_BOARD_NOT_INITIALISED
                                    ? RESULT_BOARD_NOT_INITIALISED
                                    : 0;
                                });
  if (result != RESULT_BOARD_NOT_INITIALISED)
    return;

  /* initialise the sensors? */
  if (ShowMessageBox("Sensorboard is virgin. Do you want to initialise it?",
                     "Calibrate Sensors", MB_YESNO) != IDYES)
    return;

  static constexpr const char *init_sensors[] = {
    "/opt/bin/sensorcal", "-i", nullptr
  };

  result = RunProcessDialog(UIGlobals::GetMainWindow(),
                            UIGlobals::GetDialogLook(),
                            "Calibrate Sensors", init_sensors,
                            [](int status){
                              return status == EXIT_SUCCESS
                                ? mrOK
                                : 0;
                            });
  if (result != mrOK)
    return;

  /* calibrate again */
  RunProcessDialog(UIGlobals::GetMainWindow(),
                   UIGlobals::GetDialogLook(),
                   "Calibrate Sensors", calibrate_sensors,
                   [](int status){
                     return status == STATUS_BOARD_NOT_INITIALISED
                       ? RESULT_BOARD_NOT_INITIALISED
                       : 0;
                   });
} catch (...) {
  ShowError(std::current_exception(), "Calibrate Sensors");
}

void
CalibrateTouch(UI::Display &display, UI::EventQueue &event_queue) noexcept
{
  const UI::ScopeDropMaster drop_master{display};
  const UI::ScopeSuspendEventQueue suspend_event_queue{event_queue};
  Run("/usr/bin/ov-calibrate-ts.sh");
}
