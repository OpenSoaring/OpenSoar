// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Dialogs/DialogSettings.hpp"
#include "Dialogs/Message.hpp"
#include "Dialogs/ProcessDialog.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "DisplayOrientation.hpp"
#include "Form/DataField/Listener.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/DataField/Integer.hpp"
#include "Hardware/DisplayDPI.hpp"
#include "Hardware/DisplayGlue.hpp"
#include "Hardware/RotateDisplay.hpp"
#include "Look/DialogLook.hpp"
#include "Profile/File.hpp"
#include "Profile/Map.hpp"
#include "Screen/Layout.hpp"
#include "UIGlobals.hpp"
#include "UIActions.hpp"
#include "system/FileUtil.hpp"
#include "system/Process.hpp"
#include "ui/event/KeyCode.hpp"
#include "ui/event/Timer.hpp"
#include "ui/window/Init.hpp"
#include "UtilsSettings.hpp"

#include "Language/Language.hpp"

#include "io/KeyValueFileReader.hpp"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "io/FileLineReader.hpp"

#include "OpenVario/System/OpenVarioDevice.hpp"
#include "OpenVario/DisplaySettingsWidget.hpp"
#include "OpenVario/System/Setting/RotationWidget.hpp"
#include "OpenVario/System/Setting/WifiWidget.hpp"
#include "OpenVario/System/OpenVarioTools.hpp"

#ifndef _WIN32
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <fmt/format.h>

#include <map>
#include <string>

enum ControlIndex {
  ROTATION, 
  BRIGHTNESS,

  TOUCH_CALIBRATION,

};

  static constexpr StaticEnumChoice rotation_list[] = {
    // { DisplayOrientation::DEFAULT,  _T("default") },
    {DisplayOrientation::LANDSCAPE, _T("Landscape (0°)")},
    {DisplayOrientation::PORTRAIT, _T("Portrait (90°)")},
    {DisplayOrientation::REVERSE_LANDSCAPE, _T("Rev. Landscape (180°)")},
    {DisplayOrientation::REVERSE_PORTRAIT, _T("rev. Portrait (270°)")},
    nullptr};

class DisplaySettingsWidget final : public RowFormWidget, DataFieldListener {
public:
  DisplaySettingsWidget() noexcept
      : RowFormWidget(UIGlobals::GetDialogLook()) {}

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
  // bool CheckChanged(bool &changed) noexcept;

private:
  /* methods from DataFieldListener */
  void OnModified(DataField &df) noexcept override;

  unsigned brightness;
};


void DisplaySettingsWidget::OnModified([[maybe_unused]] DataField &df) noexcept {
  if (IsDataField(ROTATION, df)) {
    // ShowMessageBox(_T("Set Rotation"), _T("Rotation"), MB_OK);
    //    ovdevice.SetRotation((DisplayOrientation)((const DataFieldEnum
    //    &)df).GetValue());
    // UI::TopWindow::SetExitValue(EXIT_RESTART);
    // UIActions::SignalShutdown(true);
  } else if (IsDataField(BRIGHTNESS, df)) {
    // const DataFieldInteger &dfi = (const DataFieldInteger &)df;
    // (DataFieldInteger*)df)
    ovdevice.SetBrightness(((const DataFieldInteger &)df).GetValue() / 10);
  }
}


void
DisplaySettingsWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                              [[maybe_unused]] const PixelRect &rc) noexcept
{

  brightness = ovdevice.brightness * 10;

#ifdef ADD_ROTATION_BUTTON
  AddButton(_("Screen Rotation"), [this](){
     return ShowRotationSettingsWidget(UIGlobals::GetMainWindow(), GetLook());
  });
#endif
  AddEnum(_("Rotation"), _("Rotation Display OpenVario"), rotation_list,
          (unsigned)ovdevice.rotation, this);

  AddInteger(_("Brightness"), _("Brightness Display OpenVario"), _T("%d%%"),
             _T("%d%%"), 10, 100, 10, brightness, this);

  AddButton(_("Calibrate Touch"), [this]() {
  // the programm exit in OpenSoar looks complete different fro OpenVarioBaseMenu
    ContainerWindow::SetExitValue(LAUNCH_TOUCH_CALIBRATE);
    UIActions::SignalShutdown(true);
    return mrOK;
  });  

// AddButton(_("Setting Brightness"), [this](){
  //   return ShowSettingBrightnessWidget(UIGlobals::GetMainWindow(), GetLook());
// });

//  uint32_t iTest = 0;
//  AddInteger(_("Brightness Test"), _("Setting Brightness."), _T("%d"), _T("%d"), 1,
//             10, 1, iTest);
}

bool 
DisplaySettingsWidget::Save([[maybe_unused]] bool &_changed) noexcept
{
bool changed = false;
//  bool restart = false;
if (SaveValueInteger(BRIGHTNESS, "Brightness", brightness)) {
  ovdevice.SetBrightness(brightness / 10);
  changed = true;
}
if (SaveValueEnum(ROTATION, ovdevice.rotation)) {
  // ovdevice.SetRotation(
  //     (DisplayOrientation)((const DataFieldEnum &)df).GetValue());
  ovdevice.SetRotation(ovdevice.rotation);
  // restart = true;
#if 0 // defined(OPENVARIO_BASEMENU)
    changed = true;
#else
  require_restart = changed = true;
#endif
}

_changed = changed;
return true;
}



bool 
ShowDisplaySettingsWidget(ContainerWindow &parent,
                              const DialogLook &look) noexcept {
  TWidgetDialog<DisplaySettingsWidget> sub_dialog(
      WidgetDialog::Full{}, (UI::SingleWindow &)parent, look,
      _T("OpenVario Display Settings"));
  sub_dialog.SetWidget();
  sub_dialog.AddButton(_("Close"), mrOK);
  return sub_dialog.ShowModal();
}

std::unique_ptr<Widget> 
CreateDisplaySettingsWidget() noexcept {
  return std::make_unique<DisplaySettingsWidget>();
}
