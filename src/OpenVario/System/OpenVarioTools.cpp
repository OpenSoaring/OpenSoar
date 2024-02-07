// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "OpenVario/System/OpenVarioTools.hpp"

#include "Dialogs/ProcessDialog.hpp"
#include "Dialogs/Message.hpp"
#include "Form/Form.hpp"

#include "UIGlobals.hpp"
#include "system/FileUtil.hpp"

#include "Widget/RowFormWidget.hpp"

#include "system/Process.hpp"
#include "ui/event/KeyCode.hpp"
#include "ui/event/Timer.hpp"
#include "ui/window/Init.hpp"
#include "util/ScopeExit.hxx"
#include "util/ConvertString.hpp"


//----------------------------------------------------------

void 
CalibrateSensors() noexcept
{
  /* make sure sensord is stopped while calibrating sensors */
  static constexpr const char *start_sensord[] = {"/bin/systemctl", "start",
                                                  "sensord.service", nullptr};
  static constexpr const char *stop_sensord[] = {"/bin/systemctl", "stop",
                                                 "sensord.service", nullptr};

  RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                   _T("Calibrate Sensors"), stop_sensord, [](int status) {
                     return status == EXIT_SUCCESS ? mrOK : 0;
                   });

  AtScopeExit() {
    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     _T("Calibrate Sensors"), start_sensord, [](int status) {
                       return status == EXIT_SUCCESS ? mrOK : 0;
                     });
  };

  /* calibrate the sensors */
  static constexpr const char *calibrate_sensors[] = {"/opt/bin/sensorcal",
                                                      "-c", nullptr};

  static constexpr int STATUS_BOARD_NOT_INITIALISED = 2;
  static constexpr int RESULT_BOARD_NOT_INITIALISED = 100;
  int result = RunProcessDialog(
      UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
      _T("Calibrate Sensors"), calibrate_sensors, [](int status) {
        return status == STATUS_BOARD_NOT_INITIALISED
                   ? RESULT_BOARD_NOT_INITIALISED
                   : 0;
      });
  if (result != RESULT_BOARD_NOT_INITIALISED)
    return;

  /* initialise the sensors? */
  if (ShowMessageBox(_T("Sensorboard is virgin. Do you want to initialise it?"),
                     _T("Calibrate Sensors"), MB_YESNO) != IDYES)
    return;

  static constexpr const char *init_sensors[] = {"/opt/bin/sensorcal", "-i",
                                                 nullptr};

  result =
      RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                       _T("Calibrate Sensors"), init_sensors, [](int status) {
                         return status == EXIT_SUCCESS ? mrOK : 0;
                       });
  if (result != mrOK)
    return;

  /* calibrate again */
  RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                   _T("Calibrate Sensors"), calibrate_sensors, [](int status) {
                     return status == STATUS_BOARD_NOT_INITIALISED
                                ? RESULT_BOARD_NOT_INITIALISED
                                : 0;
                   });
}
