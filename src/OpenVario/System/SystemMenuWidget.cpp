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
#include "system/FileUtil.hpp"

#include "Widget/RowFormWidget.hpp"

# include "system/Process.hpp"
#include "ui/event/KeyCode.hpp"
#include "ui/event/Timer.hpp"
#include "ui/window/Init.hpp"
#include "util/ScopeExit.hxx"

#include "OpenVario/SystemSettingsWidget.hpp"

#include "OpenVario/System/OpenVarioDevice.hpp"
#include "OpenVario/System/OpenVarioTools.hpp"

#include "OpenVario/System/SystemMenuWidget.hpp"

#include "system/Process.hpp"

#include <iostream>  // for std::cout << ...
#include <string>
#include <fmt/format.h>

#if  1 // Segmentation fault? def OPENVARIO_BASEMENU
# include "ui/window/ContainerWindow.hpp"
# include "UIActions.hpp"
#endif

class SystemMenuWidget final : public RowFormWidget {
public:
  SystemMenuWidget() noexcept : RowFormWidget(UIGlobals::GetDialogLook()) {}

  void SetEnabled([[maybe_unused]]bool enabled) noexcept {}

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override { return true; }

private:

};

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
  AddButton("Enable", []() {
    static constexpr const char *argv[] = {
        "/bin/sh", "-c",
        "systemctl enable --now dropbear.socket && printf '\nSSH has been enabled'",

        nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     "Enable", argv);
  });

  AddButton("Disable", []() {
    static constexpr const char *argv[] = {
        "/bin/sh", "-c",
        "systemctl disable --now dropbear.socket && printf '\nSSH has been disabled'",
        nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     "Disable", argv);
  });

  AddButton("IsEnabled", []() {
    static constexpr const char *argv[] = {
        "/bin/sh", "-c",
        "systemctl is-enabled dropbear.socket",
        nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     "IsEnabled", argv);
  });
  AddButton("IsActive", []() {
    static constexpr const char *argv[] = {
        "/bin/sh", "-c",
        "systemctl is-active dropbear.socket",
        nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     "IsActive", argv);
  });

  AddButton("GetStatus", []() {
    static constexpr const char *argv[] = {
        "/bin/sh", "-c",
        "systemctl is-enabled dropbear.socket", 
        "systemctl is-active dropbear.socket", nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     "GetStatus", argv);
  });
  AddButton("GetStatus2", []() {
    static constexpr const char *argv[] = {
        "/bin/sh", "-c",
        "/bin/systemctl is-enabled dropbear.socket", 
        "echo '\n ===== \n'", 
        "/bin/systemctl is-active dropbear.socket", nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                     "GetStatus2", argv);
  });
}

//-----------------------------------------------------------------------------
void
SystemMenuWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                          [[maybe_unused]] const PixelRect &rc) noexcept
{
  AddButton(_("Upgrade Firmware"), [this]() {
#if 0 // Segmentation fault? def OPENVARIO_BASEMENU
    exit(START_UPGRADE);
#else // OPENVARIO_BASEMENU
    ContainerWindow::SetExitValue(START_UPGRADE);
    UIActions::SignalShutdown(true);
    return START_UPGRADE;
#endif
  });

#if 1
  // the variant with command line...
  // UI::SingleWindow main_window = 
  AddButton("SSH", [this]() {
    TWidgetDialog<ScreenSSHWidget> sub_dialog(WidgetDialog::Full{},
                                              // dialog.GetMainWindow(), GetLook(),
                  UIGlobals::GetMainWindow(), GetLook(),
                                              "Enable or Disable SSH");
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
                     "Update System", argv);
  });

  AddButton(_("Calibrate Sensors"), CalibrateSensors);

  AddButton(_("Calibrate Touch"), [this]() {
// the programm exit in OpenSoar looks complete different fro OpenVarioBaseMenu
#if 0 // Segmentation fault? def OPENVARIO_BASEMENU
    exit(LAUNCH_TOUCH_CALIBRATE);
#else // OPENVARIO_BASEMENU
    ContainerWindow::SetExitValue(LAUNCH_TOUCH_CALIBRATE);
    UIActions::SignalShutdown(true);
    return mrOK;
    //        InputEvents::eventShutdown("reboot");
    // dialog.SetModalResult(LAUNCH_TOUCH_CALIBRATE);
    // dialog.SetModalResult(LAUNCH_TOUCH_CALIBRATE);

#if 0
#if defined(_WIN32)
    static constexpr const char *argv[] = { "/usr/bin/ov-calibrate-ts.sh",
          nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(), UIGlobals::GetDialogLook(),
                       "Calibrate Touch", argv);
#else
//    const UI::ScopeDropMaster drop_master{display};
//    const UI::ScopeSuspendEventQueue
        // suspend_event_queue{event_queue};
        // RunProcessDialog(UIGlobals::GetMainWindow(),
        // UIGlobals::GetDialogLook(),
    //                 "System Info", "/usr/bin/ov-calibrate-ts.sh");
//    Run("/usr/bin/ov-calibrate-ts.sh");
#endif
#endif
#endif  // OPENVARIO_BASEMENU
      });

  AddButton(_("System Info"), []() {
    static constexpr const char *argv[] = {
      "/usr/bin/system-info.sh", nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     "System Info", argv);
  });

  AddButton(_("List Dir"), []() {
    static constexpr const char *argv[] = {"/bin/ls", "-l", nullptr};

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     "List Dir", argv);
  });

  AddButton(_("Test-Process 2"), [this]() {
    StaticString<0x200> str;
    str.Format("%s/%s", ovdevice.GetHomePath().c_str(), "process.txt");
    Path output = Path(str);
#ifdef __AUGUST__
    auto ret_value = Run(
      output,
      "/home/august2111/TestProcess.sh");
#else
    int ret_value = 0;
#endif
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
      "OpenVario System Menu");
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
