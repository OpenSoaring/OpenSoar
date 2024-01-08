// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Dialogs/DialogSettings.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"
#include "Look/DialogLook.hpp"
#include "Screen/Layout.hpp"

#include "ui/window/Init.hpp"
#include "ui/window/SingleWindow.hpp"
#include "ui/event/Timer.hpp"
#include "ui/event/KeyCode.hpp"

#include "Language/Language.hpp"
#include "system/Process.hpp"
#include "system/FileUtil.hpp"
#include "Profile/Profile.hpp"
#include "Profile/File.hpp"
#include "Profile/Map.hpp"
#include "DisplayOrientation.hpp"
#include "Hardware/DisplayGlue.hpp"
#include "Hardware/DisplayDPI.hpp"
#include "Hardware/RotateDisplay.hpp"
#include "util/StaticString.hxx"

#include "OpenVario/System/System.hpp"
#include "OpenVario/FileMenuWidget.h"
#include "OpenVario/System/SystemMenuWidget.hpp"


#include "Form/DataField/Listener.hpp"
#include "Dialogs/Settings/Panels/OpenVarioConfigPanel.hpp"
#include "LocalPath.hpp"

#include <cassert>
#include <string>
#include <iostream>
#include <thread>
#include <filesystem>

#include "util/ConvertString.hpp"

bool IsOpenVarioDevice = true;

static DialogSettings dialog_settings;
static UI::SingleWindow *global_main_window;
static DialogLook *global_dialog_look;

const DialogSettings &
UIGlobals::GetDialogSettings()
{
  return dialog_settings;
}

const DialogLook &
UIGlobals::GetDialogLook()
{
  assert(global_dialog_look != nullptr);

  return *global_dialog_look;
}

UI::SingleWindow &
UIGlobals::GetMainWindow()
{
  assert(global_main_window != nullptr);

  return *global_main_window;
}


class MainMenuWidget final
  : public RowFormWidget
{
  unsigned remaining_seconds = 3;

  enum Controls {
    OPENSOAR_CLUB,
    OPENSOAR,
    XCSOAR,
    FILE,
      TEST,
    SYSTEM,
      READONLY_1,
    SHELL,
    REBOOT,
    SHUTDOWN,
    TIMER,
       READONLY_2,
  };

  UI::Display &display;
  UI::EventQueue &event_queue;

  WndForm &dialog;

  UI::Timer timer{[this](){
    if (--remaining_seconds == 0) {
      HideRow(Controls::TIMER);
      StartOpenSoar();  // StartXCSoar();
    } else {
      ScheduleTimer();
    }
  }};

public:
  MainMenuWidget(UI::Display &_display, UI::EventQueue &_event_queue,
                 WndForm &_dialog) noexcept
    :RowFormWidget(_dialog.GetLook()),
     display(_display), event_queue(_event_queue),
     dialog(_dialog) {
       GetConfigInt("timeout", remaining_seconds, ovdevice.GetConfigFile());
     }

private:
  void StartOpenSoar() noexcept {
    const UI::ScopeDropMaster drop_master{display};
    const UI::ScopeSuspendEventQueue suspend_event_queue{event_queue};
#ifdef _WIN32
    std::filesystem::path ExePath(std::filesystem::current_path());
    ExePath.append("OpenSoar.exe");
    ExePath = "D:/Projects/Binaries/OpenSoar/dev-branch/msvc2022/Release/"
              "OpenSoar.exe";
    char buf[0x200];

    snprintf(buf, sizeof(buf) - 1, "%s -fly -datapath=%s -profile=%s",
      ExePath.generic_string().c_str(),
      "D:/Data/XCSoarData",
      "D:/Data/XCSoarData/August.prf"
      );

    Run(buf);
#else
    if (File::Exists(Path(_T("/usr/bin/OpenSoar"))))
      Run("/usr/bin/OpenSoar", "-fly", "-datapath=/home/root/data/OpenSoarData");
    else
      Run("./output/UNIX/bin/OpenSoar", "-fly");
#endif
  }

  void StartXCSoar() noexcept {
    const UI::ScopeDropMaster drop_master{display};
    const UI::ScopeSuspendEventQueue suspend_event_queue{event_queue};
    Run("/usr/bin/xcsoar", "-fly", "-datapath=/home/root/data/XCSoarData");
  }

  void ScheduleTimer() noexcept {
    assert(remaining_seconds > 0);

    timer.Schedule(std::chrono::seconds{1});

    StaticString<256> buffer;
    buffer.Format(_T("Starting XCSoar in %u seconds (press any key to cancel)"),
             remaining_seconds);
    SetText(Controls::TIMER, buffer);
  }

  void CancelTimer() noexcept {
    timer.Cancel();
    remaining_seconds = 0;
    HideRow(Controls::TIMER);
  }

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;

  void Show(const PixelRect &rc) noexcept override {
    RowFormWidget::Show(rc);

    if (remaining_seconds > 0) {
      ScheduleTimer();
    }
    else {
      HideRow(Controls::TIMER);
      StartOpenSoar();  // StartXCSoar();
    }
  }

  void Hide() noexcept override {
    CancelTimer();
    RowFormWidget::Hide();
  }

  bool KeyPress(unsigned key_code) noexcept override {
    CancelTimer();

  /* ignore escape key at first menu page */
    if (key_code != KEY_ESCAPE) {
        return RowFormWidget::KeyPress(key_code);
    }
    else {
        return true;
    }
  }
};

class OpenVarioConfigPanel final : public RowFormWidget, DataFieldListener {
public:
  OpenVarioConfigPanel() noexcept : RowFormWidget(UIGlobals::GetDialogLook()) {}

  void SetEnabled(bool enabled) noexcept;

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;

private:
  /* methods from DataFieldListener */
  void OnModified(DataField &df) noexcept override;
};

void MainMenuWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                             [[maybe_unused]] const PixelRect &rc) noexcept {
  
  AddButton(_("Start OpenSoar (Club)"), [this]() {
    CancelTimer();
    StartOpenSoar();
  });

  AddButton(_("Start OpenSoar"), [this]() {
    CancelTimer();
    StartOpenSoar();
  });

  auto Btn_XCSoar = AddButton(_("Start XCSoar"), [this]() {
    CancelTimer();
    StartXCSoar();
  });

  AddButton(_("Files"), [this]() {
    CancelTimer();

    TWidgetDialog<FileMenuWidget> sub_dialog(WidgetDialog::Full{},
                                             dialog.GetMainWindow(), GetLook(),
                                             _T("OpenVario Files"));
    sub_dialog.SetWidget(display, event_queue, GetLook());
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });
  
  AddButton(_("Test"), [this]() {
    CancelTimer();
    std::unique_ptr<Widget> widget = CreateOpenVarioConfigPanel();
    Profile::LoadFile(ovdevice.GetConfigFile());
    TWidgetDialog<OpenVarioConfigPanel> sub_dialog(
        WidgetDialog::Full{}, dialog.GetMainWindow(), GetLook(),
        _T("OpenVario Test"));
    sub_dialog.SetWidget();
    sub_dialog.AddButton(_("Close"), mrOK);
    auto ret_value = sub_dialog.ShowModal();

    if (sub_dialog.GetChanged()) {
      Profile::SaveFile(ovdevice.GetConfigFile());
    }
    return ret_value;
  });

  AddButton(_("System"), [this]() {
    CancelTimer();

    TWidgetDialog<SystemMenuWidget> sub_dialog(
        WidgetDialog::Full{}, dialog.GetMainWindow(), GetLook(),
        _T("OpenVario System Settings"));
    sub_dialog.SetWidget(display, event_queue, sub_dialog);
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });

  AddReadOnly(_T(""));

  auto Btn_Shell = AddButton(_T("Shell"), [this]() { dialog.SetModalResult(LAUNCH_SHELL); });

  auto Btn_Reboot = AddButton(_T("Reboot"), []() { Run("/sbin/reboot"); });

  auto Btn_Shutdown =
      AddButton(_T("Power off"), []() { Run("/sbin/poweroff"); });

  AddReadOnly(_T("")); // Timer-Progress

  if (!IsOpenVarioDevice) {
    Btn_Shell->SetCaption(_T("Exit"));
    Btn_XCSoar->SetEnabled(false);
    Btn_Reboot->SetEnabled(false);
    Btn_Shutdown->SetEnabled(false);
  }
  HideRow(Controls::OPENSOAR_CLUB);
}

static int
Main(UI::EventQueue &event_queue, UI::SingleWindow &main_window,
     const DialogLook &dialog_look)
{
  TWidgetDialog<MainMenuWidget>
    dialog(WidgetDialog::Full{}, main_window,
           dialog_look, _T("OpenVario Base Menu"));
  dialog.SetWidget(main_window.GetDisplay(), event_queue, dialog);

  return dialog.ShowModal();
}

static int
Main()
{
  InitialiseDataPath();

  // check if this config file exists as indicator of a real OpenVario-Device:
  IsOpenVarioDevice = File::Exists(Path(_T("/boot/config.uEnv")));
  dialog_settings.SetDefaults();

  ScreenGlobalInit screen_init;
  Layout::Initialise(screen_init.GetDisplay(), {600, 800});
  // InitialiseFonts();
  
  // AllowLanguage is not in FalkLanguage
  // AllowLanguage();

  DialogLook dialog_look;
  dialog_look.Initialise();

  UI::TopWindowStyle main_style;
  main_style.Resizable();
#ifndef _WIN32
  main_style.InitialOrientation(Display::DetectInitialOrientation());
#endif

  UI::SingleWindow main_window{screen_init.GetDisplay()};
  main_window.Create(_T("OpenSoar/OpenVarioBaseMenu"), {600, 800}, main_style);
  main_window.Show();

  global_dialog_look = &dialog_look;
  global_main_window = &main_window;

  if (!IsOpenVarioDevice) {
    assert(File::Exists(ovdevice.GetConfigFile()));
  }

  int action = Main(screen_init.GetEventQueue(), main_window, dialog_look);

  main_window.Destroy();

  // DeinitialiseFonts();
  DeinitialiseDataPath();

  return action;
}

int main()
{
  /*the x-menu is waiting a second to solve timing problem with display rotation */
  std::this_thread::sleep_for(std::chrono::seconds(1));

  int action = Main();
  // save in LogFormat?
  return action;
}

#ifdef _WIN32
int RunProcessDialog(UI::SingleWindow &parent, const DialogLook &dialog_look,
                     const TCHAR *caption, const char *const *argv,
                     std::function<int(int)> on_exit = {}) noexcept
{
  return 0;
}
#endif // _WIN32
