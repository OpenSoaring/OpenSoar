// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Dialogs/DialogSettings.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Dialogs/ProcessDialog.hpp"
#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"
#include "Look/DialogLook.hpp"
#include "Screen/Layout.hpp"
#include "../test/src/Fonts.hpp"
#include "ui/window/Init.hpp"
#include "ui/window/SingleWindow.hpp"
#include "ui/event/Queue.hpp"
#include "ui/event/Timer.hpp"
#include "ui/event/KeyCode.hpp"
#include "Language/Language.hpp"
#include "system/Process.hpp"
#include "util/ScopeExit.hxx"
#include "system/FileUtil.hpp"
#include "Profile/File.hpp"
#include "Profile/Map.hpp"
#include "DisplayOrientation.hpp"
#include "Hardware/DisplayGlue.hpp"
#include "Hardware/DisplayDPI.hpp"
#include "Hardware/RotateDisplay.hpp"
#include "FileMenuWidget.h"

#include <cassert>
#include <string>
#include <iostream>
#include <thread>

static void GetConfigInt(const std::string &keyvalue, unsigned &value,
                         const TCHAR* path) {
  const Path ConfigPath(path);

  ProfileMap configuration;
  Profile::LoadFile(configuration, ConfigPath);
  configuration.Get(keyvalue.c_str(), value);
}

static void ChangeConfigInt(const std::string &keyvalue, int value,
                            const TCHAR *path) {
  const Path ConfigPath(path);

  ProfileMap configuration;

  try {
    Profile::LoadFile(configuration, ConfigPath);
  } catch (std::exception &e) {
    Profile::SaveFile(configuration, ConfigPath);
  }
  configuration.Set(keyvalue.c_str(), value);
  Profile::SaveFile(configuration, ConfigPath);
}

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

enum Buttons {
  LAUNCH_SHELL = 100,
  START_UPGRADE = 111,
};

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

class ScreenRotationWidget final
  : public RowFormWidget
{
  UI::Display &display;
  UI::EventQueue &event_queue;

public:
  ScreenRotationWidget(UI::Display &_display, UI::EventQueue &_event_queue,
                 const DialogLook &look) noexcept
    :RowFormWidget(look),
     display(_display), event_queue(_event_queue) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
  void SaveRotation(const std::string &rotationvalue);
};

/* x-menu writes the value for the display rotation to /sys because the value is also required for the console in the OpenVario.
In addition, the display rotation is saved in /boot/config.uEnv so that the Openvario sets the correct rotation again when it is restarted.*/
void
ScreenRotationWidget::SaveRotation(const std::string &rotationString)
{
   File::WriteExisting(Path(_T("/sys/class/graphics/fbcon/rotate")), (rotationString).c_str());
   int rotationInt = stoi(rotationString);
   ChangeConfigInt("rotation", rotationInt, _T("/boot/config.uEnv"));
}

void
ScreenRotationWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                             [[maybe_unused]] const PixelRect &rc) noexcept
{
 AddButton(_T("Landscape"), [this](){
	SaveRotation("0");
   Display::Rotate(DisplayOrientation::LANDSCAPE);
 });

 AddButton(_T("Portrait (90°)"), [this](){
   SaveRotation("1");
   Display::Rotate(DisplayOrientation::REVERSE_PORTRAIT);
 });

 AddButton(_T("Landscape (180°)"), [this](){
   SaveRotation("2");
   Display::Rotate(DisplayOrientation::REVERSE_LANDSCAPE);
 });

 AddButton(_T("Portrait (270°)"), [this](){
   SaveRotation("3");
   Display::Rotate(DisplayOrientation::PORTRAIT);
 });
}

class ScreenBrightnessWidget final
  : public RowFormWidget
{
  UI::Display &display;
  UI::EventQueue &event_queue;

public:
  ScreenBrightnessWidget(UI::Display &_display, UI::EventQueue &_event_queue,
                 const DialogLook &look) noexcept
    :RowFormWidget(look),
     display(_display), event_queue(_event_queue) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;

void SaveBrightness(const std::string &brightness);
};

void
ScreenBrightnessWidget::SaveBrightness(const std::string &brightness)
{
File::WriteExisting(Path(_T("/sys/class/backlight/lcd/brightness")),
                    (brightness).c_str());
}

void
ScreenBrightnessWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                                [[maybe_unused]] const PixelRect &rc) noexcept
{
  AddButton(_T("20"), [this](){
    SaveBrightness("2");
  });

  AddButton(_T("30"), [this](){
    SaveBrightness("3");
  });

  AddButton(_T("40"), [this](){
    SaveBrightness("4");
  });

  AddButton(_T("50"), [this](){
    SaveBrightness("5");
  });

  AddButton(_T("60"), [this](){
    SaveBrightness("6");
  });

  AddButton(_T("70"), [this](){
    SaveBrightness("7");
  });

  AddButton(_T("80"), [this](){
    SaveBrightness("8");
  });

  AddButton(_T("90"), [this](){
    SaveBrightness("9");
  });

  AddButton(_T("100"), [this](){
    SaveBrightness("10");
  });
}


class ScreenTimeoutWidget final
  : public RowFormWidget
{
  UI::Display &display;
  UI::EventQueue &event_queue;

public:
  ScreenTimeoutWidget(UI::Display &_display, UI::EventQueue &_event_queue,
                 const DialogLook &look) noexcept
    :RowFormWidget(look),
     display(_display), event_queue(_event_queue) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
  void SaveTimeout(int timeoutvalue);
};

void
ScreenTimeoutWidget::SaveTimeout(int timeoutInt)
{
   ChangeConfigInt("timeout", timeoutInt, _T("/boot/config.uEnv"));
}

void
ScreenTimeoutWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
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

class ScreenSSHWidget final
  : public RowFormWidget
{
  UI::Display &display;
  UI::EventQueue &event_queue;

public:
  ScreenSSHWidget(UI::Display &_display, UI::EventQueue &_event_queue,
                 const DialogLook &look) noexcept
    :RowFormWidget(look),
     display(_display), event_queue(_event_queue) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
};

void
ScreenSSHWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                         [[maybe_unused]] const PixelRect &rc) noexcept
{
  AddButton(_T("Enable") , [](){
    static constexpr const char *argv[] = {
      "/bin/sh", "-c", 
      "systemctl enable dropbear.socket && printf '\nSSH has been enabled'", 
      
      nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("Enable"), argv);
  });

  AddButton(_T("Disable") , [](){
    static constexpr const char *argv[] = {
      "/bin/sh", "-c", 
      "systemctl disable dropbear.socket && printf '\nSSH has been disabled'", 
      nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("Disable"), argv);
  });
}

class ScreenVariodWidget final
  : public RowFormWidget
{
  UI::Display &display;
  UI::EventQueue &event_queue;

public:
  ScreenVariodWidget(UI::Display &_display, UI::EventQueue &_event_queue,
                 const DialogLook &look) noexcept
    :RowFormWidget(look),
     display(_display), event_queue(_event_queue) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
};

void
ScreenVariodWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                            [[maybe_unused]] const PixelRect &rc) noexcept
{
  AddButton(_T("Enable") , [](){
    static constexpr const char *argv[] = {
      "/bin/sh", "-c", 
      "systemctl enable variod && printf '\nvariod has been enabled'", 
      nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("Enable"), argv);
  });

  AddButton(_T("Disable"), [](){
    static constexpr const char *argv[] = {
      "/bin/sh", "-c", 
      "systemctl disable variod && printf '\nvariod has been disabled'", 
      nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("Disable"), argv);
  });
}

class ScreenSensordWidget final
  : public RowFormWidget
{
  UI::Display &display;
  UI::EventQueue &event_queue;

public:
  ScreenSensordWidget(UI::Display &_display, UI::EventQueue &_event_queue,
                 const DialogLook &look) noexcept
    :RowFormWidget(look),
     display(_display), event_queue(_event_queue) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
};

void
ScreenSensordWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                             [[maybe_unused]] const PixelRect &rc) noexcept
{
  AddButton(_T("Enable"), [](){
    static constexpr const char *argv[] = {
      "/bin/sh", "-c", 
      "systemctl enable sensord && printf '\nsensord has been enabled'", 
      nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("Enable"), argv);
  });

  AddButton(_T("Disable"), [](){
    static constexpr const char *argv[] = {
      "/bin/sh", "-c", 
      "systemctl disable sensord && printf '\nsensord has been disabled'", 
      nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("Disable"), argv);
  });
}

class SystemSettingsWidget final
  : public RowFormWidget
{
  UI::Display &display;
  UI::EventQueue &event_queue;

  WndForm &dialog;

public:
  SystemSettingsWidget(UI::Display &_display, UI::EventQueue &_event_queue,
                 WndForm &_dialog) noexcept 
    :RowFormWidget(_dialog.GetLook()),
     display(_display), event_queue(_event_queue),
     dialog(_dialog) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
};

void
SystemSettingsWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                              [[maybe_unused]] const PixelRect &rc) noexcept
{
  AddButton(_("Screen Rotation"), [this](){
    TWidgetDialog<ScreenRotationWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("Display Rotation Settings"));
    sub_dialog.SetWidget(display, event_queue, GetLook());
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });

  AddButton(_("Screen Brightness"), [this](){
    TWidgetDialog<ScreenBrightnessWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("Display Brightness Settings"));
    sub_dialog.SetWidget(display, event_queue, GetLook());
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });

  AddButton(_("Autostart Timeout"), [this](){
    TWidgetDialog<ScreenTimeoutWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("Autostart Timeout"));
    sub_dialog.SetWidget(display, event_queue, GetLook());
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });

  AddButton(_("SSH"), [this](){
    TWidgetDialog<ScreenSSHWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("Enable or Disable SSH"));
    sub_dialog.SetWidget(display, event_queue, GetLook());
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });

  AddButton(_("Variod"), [this](){
    TWidgetDialog<ScreenVariodWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("Enable or Disable Variod"));
    sub_dialog.SetWidget(display, event_queue, GetLook());
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });

  AddButton(_("Sensord"), [this](){
    TWidgetDialog<ScreenSensordWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("Enable or Disable Sensord"));
    sub_dialog.SetWidget(display, event_queue, GetLook());
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });
}

class SystemMenuWidget final
  : public RowFormWidget
{
  UI::Display &display;
  UI::EventQueue &event_queue;

  WndForm &dialog;

public:
  SystemMenuWidget(UI::Display &_display, UI::EventQueue &_event_queue,
          WndForm &_dialog) noexcept
    :RowFormWidget(_dialog.GetLook()),
     display(_display), event_queue(_event_queue),
     dialog(_dialog) {}

private:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;
};

static void
CalibrateSensors() noexcept
{
  /* make sure sensord is stopped while calibrating sensors */
  static constexpr const char *start_sensord[] = {
    "/bin/systemctl", "start", "sensord.service", nullptr
  };
  static constexpr const char *stop_sensord[] = {
    "/bin/systemctl", "stop", "sensord.service", nullptr
  };

  RunProcessDialog(UIGlobals::GetMainWindow(),
                   UIGlobals::GetDialogLook(),
                   _T("Calibrate Sensors"), stop_sensord,
                   [](int status){
                     return status == EXIT_SUCCESS ? mrOK : 0;
                   });

  AtScopeExit(){
    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("Calibrate Sensors"), start_sensord,
                     [](int status){
                       return status == EXIT_SUCCESS ? mrOK : 0;
                     });
  };

  /* calibrate the sensors */
  static constexpr const char *calibrate_sensors[] = {
    "/opt/bin/sensorcal", "-c", nullptr
  };

  static constexpr int STATUS_BOARD_NOT_INITIALISED = 2;
  static constexpr int RESULT_BOARD_NOT_INITIALISED = 100;
  int result = RunProcessDialog(UIGlobals::GetMainWindow(),
                                UIGlobals::GetDialogLook(),
                                _T("Calibrate Sensors"), calibrate_sensors,
                                [](int status){
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

  static constexpr const char *init_sensors[] = {
    "/opt/bin/sensorcal", "-i", nullptr
  };

  result = RunProcessDialog(UIGlobals::GetMainWindow(),
                            UIGlobals::GetDialogLook(),
                            _T("Calibrate Sensors"), init_sensors,
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
                   _T("Calibrate Sensors"), calibrate_sensors,
                   [](int status){
                     return status == STATUS_BOARD_NOT_INITIALISED
                       ? RESULT_BOARD_NOT_INITIALISED
                       : 0;
                   });
}

void
SystemMenuWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                          [[maybe_unused]] const PixelRect &rc) noexcept
{
  AddButton(_("WiFi Settings"), [](){
    static constexpr const char *argv[] = {
      "/bin/sh", "-c", 
      "printf '\nWiFi-Settings are not implemented, yet!! \n\nIf you are interessted to help with this, write me an email: dirk@freevario.de'", 
      nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("WiFi Settings"), argv);
  });


  AddButton(_("Upgrade Firmware"), [this](){
    // dialog.SetModalResult(START_UPGRADE);
    exit(START_UPGRADE);
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
    Run("/usr/bin/ov-calibrate-ts.sh");
  });

  AddButton(_("System Settings"), [this](){
      
    TWidgetDialog<SystemSettingsWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("OpenVario System Settings"));
    sub_dialog.SetWidget(display, event_queue, sub_dialog); 
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });  

  AddButton(_("System Info"), [](){
    static constexpr const char *argv[] = {
      "/usr/bin/system-info.sh", nullptr
    };

    RunProcessDialog(UIGlobals::GetMainWindow(),
                     UIGlobals::GetDialogLook(),
                     _T("System Info"), argv);
  });
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
       GetConfigInt("timeout", remaining_seconds, _T("/boot/config.uEnv"));
     }

private:
  void StartOpenSoar() noexcept {
    const UI::ScopeDropMaster drop_master{display};
    const UI::ScopeSuspendEventQueue suspend_event_queue{event_queue};
    Run("/usr/bin/OpenSoar", "-fly", "-datapath=/home/root/data/OpenSoarData");
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

void
MainMenuWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
			[[maybe_unused]] const PixelRect &rc) noexcept
{
  AddButton(_("Start OpenSoar (Club)"), [this]() {
    CancelTimer();
    StartOpenSoar();
  });

  AddButton(_("Start OpenSoar"), [this]() {
    CancelTimer();
    StartOpenSoar();
  });

  AddButton(_("Start XCSoar"), [this]() {
    CancelTimer();
    StartXCSoar();
  });

  AddButton(_("Files"), [this](){
    CancelTimer();

    TWidgetDialog<FileMenuWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("OpenVario Files"));
    sub_dialog.SetWidget(display, event_queue, GetLook());
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });

  AddButton(_("System"), [this](){
    CancelTimer();

    TWidgetDialog<SystemMenuWidget>
      sub_dialog(WidgetDialog::Full{}, dialog.GetMainWindow(),
                 GetLook(), _T("OpenVario System Settings"));
    sub_dialog.SetWidget(display, event_queue, sub_dialog); 
    sub_dialog.AddButton(_("Close"), mrOK);
    return sub_dialog.ShowModal();
  });

  AddReadOnly(_T(""));

  AddButton(_T("Shell"), [this]() { 
    dialog.SetModalResult(LAUNCH_SHELL);
  });

  AddButton(_T("Reboot"), [](){
    Run("/sbin/reboot");
  });

  AddButton(_T("Power off") , [](){
    Run("/sbin/poweroff");
  });

  AddReadOnly(_T(""));  // Timer-Progress

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

static int
Main()
{
  dialog_settings.SetDefaults();

  ScreenGlobalInit screen_init;
  Layout::Initialise(screen_init.GetDisplay(), {600, 800});
  InitialiseFonts();

  DialogLook dialog_look;
  dialog_look.Initialise();

  UI::TopWindowStyle main_style;
  main_style.Resizable();
  #ifndef _WIN32
  main_style.InitialOrientation(Display::DetectInitialOrientation());
  #endif

  UI::SingleWindow main_window{screen_init.GetDisplay()};
  main_window.Create(_T("XCSoar/KoboMenu"), {600, 800}, main_style);
  main_window.Show();

  global_dialog_look = &dialog_look;
  global_main_window = &main_window;

  int action = Main(screen_init.GetEventQueue(), main_window, dialog_look);

  main_window.Destroy();

  DeinitialiseFonts();

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
