// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "System.hpp"
#ifndef _WIN32
# include "lib/dbus/Connection.hxx"
# include "lib/dbus/ScopeMatch.hxx"
# include "lib/dbus/Systemd.hxx"
#endif

// #include "../test/src/Fonts.hpp"
/*
#include "Dialogs/DialogSettings.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/ProcessDialog.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "DisplayOrientation.hpp"
#include "FileMenuWidget.h"
#include "Hardware/DisplayDPI.hpp"
#include "Hardware/DisplayGlue.hpp"
#include "Hardware/RotateDisplay.hpp"
#include "Look/DialogLook.hpp"
#include "Profile/File.hpp"
#include "Profile/Map.hpp"
#include "Screen/Layout.hpp"
#include "UIGlobals.hpp"
#include "Widget/RowFormWidget.hpp"
#include "system/FileUtil.hpp"
#include "system/Process.hpp"
#include "ui/event/KeyCode.hpp"
#include "ui/event/Queue.hpp"
#include "ui/event/Timer.hpp"
#include "ui/window/Init.hpp"
#include "ui/window/SingleWindow.hpp"
*/
#include "util/ScopeExit.hxx"

// #include "system/FileUtil.hpp"
// #include "system/Path.hpp"
#include "io/KeyValueFileReader.hpp"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "io/FileLineReader.hpp"
// #include "Dialogs/Error.hpp"
// #include "DisplayOrientation.hpp"
// #include "Hardware/RotateDisplay.hpp"
// 
// #include "Dialogs/DialogSettings.hpp"
// #include "Dialogs/Message.hpp"
// #include "Dialogs/WidgetDialog.hpp"
// #include "Dialogs/ProcessDialog.hpp"
// #include "Widget/RowFormWidget.hpp"
// #include "UIGlobals.hpp"
// #include "Look/DialogLook.hpp"
// #include "Screen/Layout.hpp"
// 
// #include "ui/event/KeyCode.hpp"
// #include "ui/event/Queue.hpp"
// #include "ui/event/Timer.hpp"
// #include "ui/window/Init.hpp"
// #include "ui/window/SingleWindow.hpp"
// #include "Language/Language.hpp"
// #include "system/Process.hpp"
// #include "util/ScopeExit.hxx"

#include "OV/System.hpp"

#ifndef _WIN32
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <fmt/format.h>

#include <map>

void
LoadConfigFile(std::map<std::string, std::string, std::less<>> &map, Path path)
{
  FileLineReaderA reader(path);
  KeyValueFileReader kvreader(reader);
  KeyValuePair pair;
  while (kvreader.Read(pair))
    map.emplace(pair.key, pair.value);
}

void
WriteConfigFile(std::map<std::string, std::string, std::less<>> &map, Path path)
{
  FileOutputStream file(path);
  BufferedOutputStream buffered(file);

  for (const auto &i : map)
    buffered.Fmt("{}={}\n", i.first, i.second);

  buffered.Flush();
  file.Commit();
}

uint_least8_t
OpenvarioGetBrightness() noexcept
{
  char line[4];
  int result = 10;

  if (File::ReadString(Path(_T("/sys/class/backlight/lcd/brightness")), line, sizeof(line))) {
    result = atoi(line);
  }

  return result;
}

void
OpenvarioSetBrightness(uint_least8_t value) noexcept
{
  if (value < 1) { value = 1; }
  if (value > 10) { value = 10; }

  File::WriteExisting(Path(_T("/sys/class/backlight/lcd/brightness")), fmt::format_int{value}.c_str());
}

DisplayOrientation
OpenvarioGetRotation()
{
  std::map<std::string, std::string, std::less<>> map;
  LoadConfigFile(map, Path(_T("/boot/config.uEnv")));

  uint_least8_t result;
  result = map.contains("rotation") ? std::stoi(map.find("rotation")->second) : 0;

  switch (result) {
  case 0: return DisplayOrientation::LANDSCAPE;
  case 1: return DisplayOrientation::REVERSE_PORTRAIT;
  case 2: return DisplayOrientation::REVERSE_LANDSCAPE;
  case 3: return DisplayOrientation::PORTRAIT;
  default: return DisplayOrientation::DEFAULT;
  }
}

void
OpenvarioSetRotation(DisplayOrientation orientation)
{
  std::map<std::string, std::string, std::less<>> map;

  Display::Rotate(orientation);

  int rotation = 0; 
  switch (orientation) {
  case DisplayOrientation::DEFAULT:
  case DisplayOrientation::LANDSCAPE:
    break;
  case DisplayOrientation::REVERSE_PORTRAIT:
    rotation = 1;
    break;
  case DisplayOrientation::REVERSE_LANDSCAPE:
    rotation = 2;
    break;
  case DisplayOrientation::PORTRAIT:
    rotation = 3;
    break;
  };

  File::WriteExisting(Path(_T("/sys/class/graphics/fbcon/rotate")), fmt::format_int{rotation}.c_str());

  LoadConfigFile(map, Path(_T("/boot/config.uEnv")));
  map.insert_or_assign("rotation", fmt::format_int{rotation}.c_str());
  WriteConfigFile(map, Path(_T("/boot/config.uEnv")));
}

// #ifndef _WIN32
#if 0 
SSHStatus
OpenvarioGetSSHStatus()
{
  auto connection = ODBus::Connection::GetSystem();

  if (Systemd::IsUnitEnabled(connection, "dropbear.socket")) {
    return SSHStatus::ENABLED;
  } else if (Systemd::IsUnitActive(connection, "dropbear.socket")) {
    return SSHStatus::TEMPORARY;
  } else {
    return SSHStatus::DISABLED;
  }
}

void
OpenvarioEnableSSH(bool temporary)
{
  auto connection = ODBus::Connection::GetSystem();
  const ODBus::ScopeMatch job_removed_match{connection, Systemd::job_removed_match};

  if (temporary)
    Systemd::DisableUnitFile(connection, "dropbear.socket");
  else
    Systemd::EnableUnitFile(connection, "dropbear.socket");

  Systemd::StartUnit(connection, "dropbear.socket");
}

void
OpenvarioDisableSSH()
{
  auto connection = ODBus::Connection::GetSystem();
  const ODBus::ScopeMatch job_removed_match{connection, Systemd::job_removed_match};

  Systemd::DisableUnitFile(connection, "dropbear.socket");
  Systemd::StopUnit(connection, "dropbear.socket");
}
#endif  // _WIN32


void GetConfigInt(const std::string &keyvalue, unsigned &value,
                         const TCHAR* path) {
  const Path ConfigPath(path);

  ProfileMap configuration;
  Profile::LoadFile(configuration, ConfigPath);
  configuration.Get(keyvalue.c_str(), value);
}

void ChangeConfigInt(const std::string &keyvalue, int value,
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


static void CalibrateSensors() noexcept {
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

//----------------------------------------------------------
//----------------------------------------------------------
//----------------------------------------------------------

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
//----------------------------------------------------------
//----------------------------------------------------------
//----------------------------------------------------------

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

