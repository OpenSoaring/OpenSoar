// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

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
#include "system/Process.hpp"
#include "ui/event/KeyCode.hpp"
// #include "ui/event/Queue.hpp"
#include "ui/event/Timer.hpp"
#include "ui/window/Init.hpp"
// #include "ui/window/SingleWindow.hpp"

#include "Language/Language.hpp"

#include "io/KeyValueFileReader.hpp"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "io/FileLineReader.hpp"

#include "OpenVario/System/System.hpp"
#include "OpenVario/System/Setting/TimeoutWidget.hpp"

#ifndef __MSVC__
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <fmt/format.h>

#include <map>
#include <string>

//----------------------------------------------------------
//----------------------------------------------------------
//----------------------------------------------------------

void
SettingTimeoutWidget::SaveTimeout(int timeoutInt)
{
  ChangeConfigInt("timeout", timeoutInt, ovdevice.GetSettingsConfig());
}

void
SettingTimeoutWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                             [[maybe_unused]] const PixelRect &rc) noexcept

{
  AddButton(_T("immediately"), [this](){
    SaveTimeout(0);
        static constexpr const char *argv[] = {
      "/bin/sh", "-c", 
      "echo Automatic timeout was set to 0s", 
      nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("immediately"), argv);
  });

  AddButton(_T("1s"), [this](){
    SaveTimeout(1);
    static constexpr const char *argv[] = {
      "/bin/sh", "-c", 
      "echo Automatic timeout was set to 1s", 
      nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("1s"), argv);
  });

  AddButton(_T("3s"), [this](){
    SaveTimeout(3);
    static constexpr const char *argv[] = {
      "/bin/sh", "-c", 
      "echo Automatic timeout was set to 3s", 
      nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("3s"), argv);
  });

  AddButton(_T("5s"), [this](){
    SaveTimeout(5);
    static constexpr const char *argv[] = {
      "/bin/sh", "-c", 
      "echo Automatic timeout was set to 5s", 
      nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("5s"), argv);
  });

  AddButton(_T("10s"), [this](){
    SaveTimeout(10);
    static constexpr const char *argv[] = {
      "/bin/sh", "-c", 
      "echo Automatic timeout was set to 10s", 
      nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("10s"), argv);
  });

  AddButton(_T("30s"), [this](){
    SaveTimeout(30);
    static constexpr const char *argv[] = {
      "/bin/sh", "-c", 
      "echo Automatic timeout was set to 30s", 
      nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("30s"), argv);
  });
}

//----------------------------------------------------------
//----------------------------------------------------------
//----------------------------------------------------------
#if 0
void
SystemSettingsWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                              [[maybe_unused]] const PixelRect &rc) noexcept
{
  AddButton(_("Setting Rotation"), [this](){
    TWidgetDialog<SettingRotationWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("Display Rotation Settings"));
    sub_dialog.SetWidget(display, event_queue, GetLook());
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });

  AddButton(_("Setting Brightness"), [this](){
    TWidgetDialog<SettingBrightnessWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("Display Brightness Settings"));
    sub_dialog.SetWidget(display, event_queue, GetLook());
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });

  AddButton(_("Autostart Timeout"), [this](){
    TWidgetDialog<SettingTimeoutWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("Autostart Timeout"));
    sub_dialog.SetWidget(display, event_queue, GetLook());
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });

  AddButton(_("SSH"), [this](){
    TWidgetDialog<SettingSSHWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("Enable or Disable SSH"));
    sub_dialog.SetWidget(display, event_queue, GetLook());
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });

  AddButton(_("Variod"), [this](){
    TWidgetDialog<SettingVariodWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("Enable or Disable Variod"));
    sub_dialog.SetWidget(display, event_queue, GetLook());
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });

  AddButton(_("Sensord"), [this](){
    TWidgetDialog<SettingSensordWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("Enable or Disable Sensord"));
    sub_dialog.SetWidget(display, event_queue, GetLook());
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });
}

#endif