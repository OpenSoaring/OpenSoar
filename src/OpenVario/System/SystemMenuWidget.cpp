// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef IS_OPENVARIO
// don't use (and compile) this code outside an OpenVario project!

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
// #include "UIActions.hpp"
#include "system/FileUtil.hpp"

#include "Widget/RowFormWidget.hpp"

#if !defined(_WIN32)
# include "system/Process.hpp"
#endif
#include "ui/event/KeyCode.hpp"
#include "ui/event/Timer.hpp"
#include "ui/window/Init.hpp"
#include "ui/window/ContainerWindow.hpp"
#include "util/ScopeExit.hxx"

#include "OpenVario/SystemSettingsWidget.hpp"
#include "OpenVario/System/OpenVarioDevice.hpp"
#include "OpenVario/System/SystemMenuWidget.hpp"

#include "system/Process.hpp"

#include <iostream>
#include <string>
#include <fmt/format.h>

class SystemMenuWidget final : public RowFormWidget {
public:
  SystemMenuWidget() noexcept : RowFormWidget(UIGlobals::GetDialogLook()) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  // void CalibrateSensors() noexcept;
};

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
class ScreenSSHWidget final : public RowFormWidget {

public:
  ScreenSSHWidget() noexcept : RowFormWidget(UIGlobals::GetDialogLook()) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
};

void ScreenSSHWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                              [[maybe_unused]] const PixelRect &rc) noexcept {
  AddButton(_T("Enable"), []() {
    static constexpr const char *argv[] = {
        "/bin/sh", "-c",
        "systemctl enable --now dropbear.socket && printf '\nSSH has been enabled'",

        nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     _T("Enable"), argv);
  });

  AddButton(_T("Disable"), []() {
    static constexpr const char *argv[] = {
        "/bin/sh", "-c",
        "systemctl disable --now dropbear.socket && printf '\nSSH has been disabled'",
        nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     _T("Disable"), argv);
  });

  AddButton(_T("IsEnabled"), []() {
    static constexpr const char *argv[] = {
        "/bin/sh", "-c",
        "systemctl is-enabled dropbear.socket",
        nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     _T("IsEnabled"), argv);
  });
  AddButton(_T("IsActive"), []() {
    static constexpr const char *argv[] = {
        "/bin/sh", "-c",
        "systemctl is-active dropbear.socket",
        nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     _T("IsActive"), argv);
  });

  AddButton(_T("GetStatus"), []() {
    static constexpr const char *argv[] = {
        "/bin/sh", "-c",
        "systemctl is-enabled dropbear.socket", 
        "systemctl is-active dropbear.socket", nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     _T("GetStatus"), argv);
  });
  AddButton(_T("GetStatus2"), []() {
    static constexpr const char *argv[] = {
        "/bin/sh", "-c",
        "/bin/systemctl is-enabled dropbear.socket", 
        "echo '\n ===== \n'", 
        "/bin/systemctl is-active dropbear.socket", nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     _T("GetStatus2"), argv);
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

#if 1
  // the variant with command line...
  // UI::SingleWindow main_window = 
  AddButton(_T("SSH"), [this]() {
    TWidgetDialog<ScreenSSHWidget> sub_dialog(WidgetDialog::Full{},
                                              // dialog.GetMainWindow(), GetLook(),
                  UIGlobals::GetMainWindow(), GetLook(),
                                              _T("Enable or Disable SSH"));
    sub_dialog.SetWidget();
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });
#endif

  AddButton(_("Update System"), [](){
    static constexpr const char *argv[] = {
      "/usr/bin/update-system.sh", nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("Update System"), argv);
  });

  AddButton(_("Calibrate Sensors"), CalibrateSensors);

  AddButton(_("Calibrate Touch"), [this]() {
    // dialog.SetModalResult(LAUNCH_TOUCH_CALIBRATE);
    // dialog.SetModalResult(LAUNCH_TOUCH_CALIBRATE);
    ContainerWindow::SetExitValue(LAUNCH_TOUCH_CALIBRATE);
//    UIActions::SignalShutdown(false);


    exit(LAUNCH_TOUCH_CALIBRATE);
#if 0
#if defined(_WIN32)
    static constexpr const char *argv[] = { "/usr/bin/ov-calibrate-ts.sh",
          nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                       _T("Calibrate Touch"), argv);
#else
//    const UI::ScopeDropMaster drop_master{display};
//    const UI::ScopeSuspendEventQueue
        // suspend_event_queue{event_queue};
        // RunProcessDialog(UIGlobals::GetMainWindow(),
        // UIGlobals::GetDialogLook(),
    //                 _T("System Info"), "/usr/bin/ov-calibrate-ts.sh");
//    Run("/usr/bin/ov-calibrate-ts.sh");
#endif
#endif
  });

  AddButton(_("System Info"), []() {
    static constexpr const char *argv[] = {
      "/usr/bin/system-info.sh", nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("System Info"), argv);
  });

  AddButton(_("List Dir"), []() {
    static constexpr const char *argv[] = {"/bin/ls", "-l", nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("List Dir"), argv);
  });

  AddButton(_("Test-Process 2"), [this]() {
    StaticString<0x200> str;
    str.Format(_T("%s/%s"), ovdevice.GetHomePath().c_str(), _T("process.txt"));
    Path output = Path(str);
    std::cout << "FileName: " << output.ToUTF8() << std::endl;

    auto ret_value = Run(
      output,
      "/home/august2111/TestProcess.sh");

    char buffer[0x100];
    File::ReadString(output, buffer, sizeof(buffer));
    std::string xx = buffer; 
    xx += "\nreturn value: " + std::to_string(ret_value);  
    File::WriteExisting(output, xx.c_str());
    return ret_value;
    });
}

bool
ShowSystemMenuWidget(ContainerWindow  &parent,
  const DialogLook &look) noexcept
{
  TWidgetDialog<SystemMenuWidget> sub_dialog(
      WidgetDialog::Full{}, (UI::SingleWindow &) parent, look,
      _T("OpenVario System Menu"));
  sub_dialog.SetWidget();
  sub_dialog.AddButton(_("Close"), mrOK);
  return sub_dialog.ShowModal();
}

std::unique_ptr<Widget>
CreateSystemMenuWidget() noexcept
{
  return std::make_unique<SystemMenuWidget>();
}

#endif
