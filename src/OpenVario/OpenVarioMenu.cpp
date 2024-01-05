// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Dialogs/DialogSettings.hpp"
// #include "Dialogs/Message.hpp"
#include "Dialogs/WidgetDialog.hpp"
// #include "Dialogs/ProcessDialog.hpp"
#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"
#include "Look/DialogLook.hpp"
#include "Screen/Layout.hpp"
// #include "../test/src/Fonts.hpp"
// #include "Fonts.hpp"
#include "ui/window/Init.hpp"
#include "ui/window/SingleWindow.hpp"
// #include "ui/event/Queue.hpp"
#include "ui/event/Timer.hpp"
#include "ui/event/KeyCode.hpp"
#include "Language/Language.hpp"
#include "system/Process.hpp"
// #include "util/ScopeExit.hxx"
#include "system/FileUtil.hpp"
#include "Profile/File.hpp"
#include "Profile/Map.hpp"
#include "DisplayOrientation.hpp"
#include "Hardware/DisplayGlue.hpp"
#include "Hardware/DisplayDPI.hpp"
#include "Hardware/RotateDisplay.hpp"

#include "OpenVario/System/System.hpp"
#include "OpenVario/FileMenuWidget.h"
#include "OpenVario/System/SystemMenuWidget.hpp"

#include <cassert>
#include <string>
#include <iostream>
#include <thread>

#include "util/StaticString.hxx"

bool IsOpenVarioDevice = true;

template<typename T>
static void ChangeConfigString(const std::string &keyvalue, T value,
                               const std::string &path) {
  const Path ConfigPath(path.c_str());

  ProfileMap configuration;

  try {
    Profile::LoadFile(configuration, ConfigPath);
  } catch (std::exception &e) {
    Profile::SaveFile(configuration, ConfigPath);
  }
  configuration.Set(keyvalue.c_str(), value);
  Profile::SaveFile(configuration, ConfigPath);
}

// enum Buttons {
//   LAUNCH_SHELL = 100,
//   START_UPGRADE = 111,
// };

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
      StartOpenSoar();
      // StartXCSoar();
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
       GetConfigInt("timeout", remaining_seconds, ConfigFile);
     }

private:
  void StartOpenSoar() noexcept {
    const UI::ScopeDropMaster drop_master{display};
    const UI::ScopeSuspendEventQueue suspend_event_queue{event_queue};
    if (File::Exists(Path(_T("/usr/bin/OpenSoar"))))
      Run("/usr/bin/OpenSoar", "-fly", "-datapath=/home/root/data/OpenSoarData");
    else
      Run("./output/UNIX/bin/OpenSoar", "-fly");
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
      StartOpenSoar();
      // StartXCSoar();
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

  AddButton(_T("Shell"), [this]() { dialog.SetModalResult(LAUNCH_SHELL); });

  auto Btn_Reboot = AddButton(_T("Reboot"), []() { Run("/sbin/reboot"); });

  auto Btn_Shutdown =
      AddButton(_T("Power off"), []() { Run("/sbin/poweroff"); });

  AddReadOnly(_T("")); // Timer-Progress

  if (!IsOpenVarioDevice) {
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
           dialog_look, _T("OpenVario"));
  dialog.SetWidget(main_window.GetDisplay(), event_queue, dialog);

  return dialog.ShowModal();
}

#include <stdarg.h>
#define MAX_PATH 0x100
void debugln(const char *fmt, ...) noexcept {
  char buf[MAX_PATH];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
  va_end(ap);

  strcat(buf, "\n");
  printf(buf);
}

static int
Main()
{
  IsOpenVarioDevice = File::Exists(ConfigFile);
  dialog_settings.SetDefaults();

  ScreenGlobalInit screen_init;
  Layout::Initialise(screen_init.GetDisplay(), {600, 800});
  // InitialiseFonts();

  DialogLook dialog_look;
  dialog_look.Initialise();

  UI::TopWindowStyle main_style;
  main_style.Resizable();
#ifndef _WIN32
  main_style.InitialOrientation(Display::DetectInitialOrientation());
#endif

  UI::SingleWindow main_window{screen_init.GetDisplay()};
  main_window.Create(_T("XCSoar/OpenVarioMenu"), {600, 800}, main_style);
  main_window.Show();

  global_dialog_look = &dialog_look;
  global_main_window = &main_window;

  if (!IsOpenVarioDevice) {
    // StaticString<0x100> Home;
    //Home.SetUTF8(getenv("HOME"));
    // Home = _T("/home/august2111");
    debugln("HOME = %s", getenv("HOME"));

    ConfigFile = Path(_T("./config.uEnv"));
        // AllocatedPath::Build(Path(Home), Path(_T("/config.uEnv")));
        // AllocatedPath::Build(Path(_T("/home/august2111")), Path(_T("/config.uEnv")));
        // AllocatedPath::Build(Path(), Path(_T("/config.uEnv")));
    debugln("ConfigFile: %s", ConfigFile.c_str());
    // #endif
    if (!File::Exists(ConfigFile)) {
        debugln("ConfigFile does not exist", ConfigFile.c_str());
        File::CreateExclusive(ConfigFile);
        if (!File::Exists(ConfigFile))
          debugln("still ConfigFile does not exist", ConfigFile.c_str());
     }
  }

  int action = Main(screen_init.GetEventQueue(), main_window, dialog_look);

  main_window.Destroy();

  // DeinitialiseFonts();

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
