
#include "Dialogs/DialogSettings.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/ProcessDialog.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "DisplayOrientation.hpp"
#include "Hardware/DisplayDPI.hpp"
#include "Hardware/DisplayGlue.hpp"
#include "Hardware/RotateDisplay.hpp"
#include "Look/DialogLook.hpp"
#include "Profile/File.hpp"
#include "Profile/Map.hpp"
#include "Screen/Layout.hpp"
#include "UIGlobals.hpp"
// #include "Widget/RowFormWidget.hpp"
#include "system/FileUtil.hpp"

#if !defined(_WIN32)
# include "system/Process.hpp"
#endif
#include "ui/event/KeyCode.hpp"
// #include "ui/event/Queue.hpp"
#include "ui/event/Timer.hpp"
#include "ui/window/Init.hpp"
// #include "ui/window/SingleWindow.hpp"
#include "util/ScopeExit.hxx"

#include "OpenVario/FileMenuWidget.h"
#include "OpenVario/System/System.hpp"
#include "OpenVario/System/SystemSettingsWidget.hpp"
#include "OpenVario/System/SystemMenuWidget.hpp"
#include "Dialogs/Settings/Panels/OpenVarioConfigPanel.hpp"

#include <string>
#include <fmt/format.h>

// void 
// SystemMenuWidget::CalibrateSensors() noexcept
static void 
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

//-----------------------------------------------------------------------------
void
SystemMenuWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                          [[maybe_unused]] const PixelRect &rc) noexcept
{
  AddButton(_("Upgrade Firmware"), [this]() {
    // dialog.SetModalResult(START_UPGRADE);
    exit(START_UPGRADE);
  });

  AddButton(_("WiFi Settings"), []() {
    static constexpr const char *argv[] = {
      "/bin/sh", "-c", 
      "printf '\\nWiFi-Settings are not implemented, yet!! \\n\\nIf you are "
      "interessted to help with this, write me an email: dirk@freevario.de'", 
      nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("WiFi Settings"), argv);
  });


  AddButton(_("Update System"), [](){
    static constexpr const char *argv[] = {
      "/usr/bin/update-system.sh", nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("Update System"), argv);
  });

  AddButton(_("Calibrate Sensors"), CalibrateSensors);
  AddButton(_("Calibrate Touch"), [this](){
    const UI::ScopeDropMaster drop_master{display};
    const UI::ScopeSuspendEventQueue suspend_event_queue{event_queue};
#if !defined(_WIN32)
    // RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
    //                 _T("System Info"), "/usr/bin/ov-calibrate-ts.sh");
    Run("/usr/bin/ov-calibrate-ts.sh");
#endif
  });

  AddButton(_("System Settings"), [this](){
      
    TWidgetDialog<SystemSettingsWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("OpenVario System Settings"));
    sub_dialog.SetWidget(display, event_queue, sub_dialog); 
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });  

  AddButton(_("Device Settings"), [this]() {
    std::unique_ptr<Widget> widget = CreateOpenVarioConfigPanel();

    TWidgetDialog<OpenVarioConfigPanel> sub_dialog(
        WidgetDialog::Full{}, dialog.GetMainWindow(), GetLook(),
        _T("OpenVario System Settings"));
    sub_dialog.SetWidget();
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });

  AddButton(_("System Info"), []() {
    static constexpr const char *argv[] = {
      "/usr/bin/system-info.sh", nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("System Info"), argv);
  });
}

