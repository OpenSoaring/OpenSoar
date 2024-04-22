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

#include "OpenVario/FileMenuWidget.hpp"
#include "OpenVario/ExtraWidget.hpp"
#include "OpenVario/DisplaySettingsWidget.hpp"
#include "OpenVario/SystemSettingsWidget.hpp"

#include "OpenVario/System/OpenVarioDevice.hpp"
#include "OpenVario/System/SystemMenuWidget.hpp"

#include "util/ConvertString.hpp"
#include "LocalPath.hpp"
#include "LogFile.hpp"
#include "Dialogs/Message.hpp"

#include <cassert>
#include <string>
#include <iostream>
#include <thread>
#include <filesystem>

static DialogSettings dialog_settings;
static UI::SingleWindow *global_main_window;
static DialogLook *global_dialog_look;
static unsigned StartTimeout = 1;

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

  UI::Display &display;
  UI::EventQueue &event_queue;

  WndForm &dialog;

  UI::Timer timer{[this](){
    if (remaining_seconds == (unsigned)-1)
      CancelTimer();
    else if (--remaining_seconds == 0) {
      progress_timer->Hide();
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
       ovdevice.LoadSettings();
       remaining_seconds = StartTimeout == 0 ? 0 : ovdevice.timeout;
     }

private:
  int StartSoarExe(std::string_view exe,
                    std::filesystem::path _datapath = "") noexcept;

  int StartOpenSoar() noexcept {
    std::filesystem::path datapath = "";
    if (ovdevice.settings.find("OpenSoarData") != ovdevice.settings.end())
      datapath = ovdevice.settings.find("OpenSoarData")->second;
    else
      datapath = ovdevice.GetDataPath().ToUTF8() + "/OpenSoarData/";

    int ret_value = StartSoarExe("OpenSoar", datapath);

    // after OpenSoar the settings can be changed
    ovdevice.LoadSettings();
    switch (ret_value) { 
    case 200:  // simple exit from OpenSoar, go in menu
      break;
    case 201:
      Run("/sbin/reboot");
      break;
    case 202:
      Run("/sbin/poweroff");
      break;
    case LAUNCH_SHELL: // "Exit to shell'
      exit(100);
    case START_UPGRADE:
    case LAUNCH_TOUCH_CALIBRATE:
      exit(ret_value);
    default:
      break;
    }
    return ret_value;
  }

  int StartXCSoar() noexcept {
    std::filesystem::path datapath = "";
    if (ovdevice.settings.find("XCSoarData") != ovdevice.settings.end())
      datapath = ovdevice.settings.find("XCSoarData")->second;
    else 
      datapath = ovdevice.GetDataPath().ToUTF8() + "/XCSoarData/";
    return StartSoarExe("xcsoar", datapath);
  }

    void ScheduleTimer() noexcept {
    assert(remaining_seconds > 0);

    timer.Schedule(std::chrono::seconds{1});

    StaticString<256> buffer;
    buffer.Format("Starting XCSoar in %u seconds (press any key to cancel)",
             remaining_seconds);
    progress_timer->SetText(buffer);
  }

  void CancelTimer() noexcept {
    timer.Cancel();
    remaining_seconds = 0;
    progress_timer->Hide();
  }

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent,
               const PixelRect &rc) noexcept override;

  void Show(const PixelRect &rc) noexcept override {
    RowFormWidget::Show(rc);

    switch (remaining_seconds) {
    case (unsigned)-1:
      progress_timer->Hide();
      break; // Do Nothing !  
    case 0:
      progress_timer->Hide();
      StartOpenSoar(); // StartXCSoar();
      break;
    default:
      ScheduleTimer();
      break;
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

  WndProperty *progress_timer = nullptr;
};

int
MainMenuWidget::StartSoarExe(std::string_view _exe,
                  std::filesystem::path _datapath) noexcept {
  int ret_value = 0;
  const UI::ScopeDropMaster drop_master{display};
  const UI::ScopeSuspendEventQueue suspend_event_queue{event_queue};

  std::filesystem::path ExePath = ovdevice.GetBinPath().append(_exe);
#ifdef _WIN32
  ExePath += ".exe";
#endif
  // std::string for memory storage reasons
  std::string exe = ExePath.string().c_str();
  LogFormat("ExePath = %s", exe.c_str());
  printf("ExePath = %s\n", exe.c_str());
  if (File::Exists(Path(ExePath.c_str()))) {
    std::vector<const char *> ArgList;
    ArgList.reserve(10);
    ArgList = {exe.c_str(), "-fly"};

    //=============== Datapath =======================
    NarrowString<0x200> datapath("-datapath=");
    if (_datapath.empty()) {
        _datapath = GetPrimaryDataPath().c_str();
    } else {
        datapath.append(_datapath.generic_string());
        datapath.append("/");  // last delimiter
    }
    if (!datapath.empty())
      ArgList.push_back(datapath.c_str());

    //=============== Profile =======================
    if (ovdevice.settings.find("MainProfile") != ovdevice.settings.end()) {
      NarrowString<0x80> profile("");
      profile = "-profile=";
      std::string str = ovdevice.settings.find("MainProfile")->second;
      profile += ovdevice.settings.find("MainProfile")->second;
      ArgList.push_back(profile.c_str());
    }

    //=============== Profile =======================
#ifdef _WIN32  // OpenVario needs no format field!
    if (ovdevice.settings.find("Format") != ovdevice.settings.end()) {
      NarrowString<0x10> format("");
      format = "-";
      format += ovdevice.settings.find("Format")->second;
      ArgList.push_back(format.c_str());
      //    LogFormat("format = %s", format.c_str());
    } else {
      ArgList.push_back("-1400x700");
    }
#endif
    //=============== End of Parameter =======================
    ArgList.push_back(nullptr);
    //========================================================
    ret_value = Run(&ArgList[0]);
  } else { // ExePath doesnt exist!
    LogFormat("Program file '%s' doesnt exist!", ExePath.c_str());
#ifdef _WIN32
    // with Linux ShowMessageBox -> crash!
    ShowMessageBox(_("Program file doesnt exist!"), "Run",
                   MB_OK | MB_ICONEXCLAMATION);
#endif
  }
  return ret_value;
}

void MainMenuWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                             [[maybe_unused]] const PixelRect &rc) noexcept {
  
  [[maybe_unused]] auto Btn_Club = // unused
      AddButton(_("Start OpenSoar (Club)"), [this]() {
    CancelTimer();
    StartOpenSoar();
  });

  AddButton(_("Start OpenSoar"), [this]() {
    CancelTimer();
    StartOpenSoar();
  });

  [[maybe_unused]] auto Btn_XCSoar = AddButton(_("Start XCSoar"), [this]() {
    CancelTimer();
    StartXCSoar();
  });

  //----------------------------------------------------------
  AddReadOnly(""); // split between start cmds and setting
  //----------------------------------------------------------

  AddButton(_("File Transfers"), [this]() {
    CancelTimer();
    return ShowFileMenuWidget(UIGlobals::GetMainWindow(), GetLook());
  });
  
  AddButton(_("Display Settings"), [this]() {
    CancelTimer();
    return ShowDisplaySettingsWidget(UIGlobals::GetMainWindow(), GetLook());
  });

  AddButton(_("System Settings"), [this]() {
    CancelTimer();
    return ShowSystemSettingsWidget(UIGlobals::GetMainWindow(), GetLook());
  });

  AddButton(_("OpenVario Extra menu "), [this]() {
    CancelTimer();
    return ShowExtraWidget(UIGlobals::GetMainWindow(), GetLook());
  });

  AddButton(_("OpenVario Placeholder"), [this]() {
    CancelTimer();
    return ShowSystemMenuWidget(UIGlobals::GetMainWindow(), GetLook());

//    TWidgetDialog<SystemMenuWidget> sub_dialog(
//        WidgetDialog::Full{}, UIGlobals::GetMainWindow(), GetLook(),
//        "OpenVario Placeholder Settings");
//    sub_dialog.SetWidget(display, event_queue, sub_dialog);
//    // sub_dialog.SetWidget();
//    sub_dialog.AddButton(_("Close"), mrOK);
//    return sub_dialog.ShowModal();
  });

  //----------------------------------------------------------
  AddReadOnly(""); // split setting and switch off cmds
  //----------------------------------------------------------

  auto Btn_Shell = AddButton("Exit to Shell",
                             [this]() { dialog.SetModalResult(LAUNCH_SHELL); });

#ifndef RELEASE_VERSION
  AddButton("Exit to Shell (with Wait)",
            [this]() { dialog.SetModalResult(LAUNCH_SHELL_STOP); });
#endif

  auto Btn_Reboot =
      AddButton("Reboot", []() { Run("/sbin/reboot"); });
  // auto Btn_Reboot = AddButton("Reboot", []() { Run("/sbin/reboot"); });
  
  auto Btn_Shutdown =
      AddButton("Power off", []() { Run("/sbin/poweroff"); });
      // AddButton("Power off", []() { "/sbin/poweroff"); });

  //----------------------------------------------------------
  progress_timer = RowFormWidget::Add("", "", true);
  // AddReadOnly(""); // Timer-Progress
  //----------------------------------------------------------

  if (!ovdevice.IsReal()) {
    Btn_Shell->SetCaption("Exit");
    Btn_Reboot->SetEnabled(false);
    Btn_Shutdown->SetEnabled(false);
  }
  // Btn_XCSoar->SetEnabled(File::Exists);
  
  progress_timer->Hide();
}

static int
Main(UI::EventQueue &event_queue, UI::SingleWindow &main_window,
     const DialogLook &dialog_look)
{
  TWidgetDialog<MainMenuWidget>
    dialog(WidgetDialog::Full{}, main_window,
           dialog_look, "OpenVario Base Menu");
  dialog.SetWidget(main_window.GetDisplay(), event_queue, dialog);

  return dialog.ShowModal();
}

static int
Main()
{
  InitialiseDataPath();
  ovdevice.Initialise();

  dialog_settings.SetDefaults();

  ScreenGlobalInit screen_init;
  Layout::Initialise(screen_init.GetDisplay(), {600, 800});
  // InitialiseFonts();
  
  // AllowLanguage is not in FalkLanguage
  AllowLanguage();

  DialogLook dialog_look;
  dialog_look.Initialise();

  UI::TopWindowStyle main_style;
  main_style.Resizable();
#ifndef _WIN32
  main_style.InitialOrientation(Display::DetectInitialOrientation());
#endif

  UI::SingleWindow main_window{screen_init.GetDisplay()};
  main_window.Create("OpenSoar/OpenVarioBaseMenu", {600, 800}, main_style);
  main_window.Show();

  global_dialog_look = &dialog_look;
  global_main_window = &main_window;

  if (!ovdevice.IsReal()) {
    assert(File::Exists(ovdevice.GetSettingsConfig()));
  }

  int action = Main(screen_init.GetEventQueue(), main_window, dialog_look);

  main_window.Destroy();

  // DeinitialiseFonts();
  DeinitialiseDataPath();

  return action;
}

int 
main(int argc, char *argv[])
{
  if (argc > 0)
    ovdevice.SetBinPath(argv[0]);
  if (argc > 1)
    StartTimeout = std::strtoul(argv[1], nullptr, 10);
  /* the OpenVarioBaseMenu is waiting a second to solve timing problem with
     display rotation */
  std::this_thread::sleep_for(std::chrono::seconds(1));

  int action = Main();
  // save in LogFormat?
  return action;
}
