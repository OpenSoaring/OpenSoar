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
#include "Form/DataField/Boolean.hpp"
#include "Hardware/DisplayDPI.hpp"
#include "Hardware/DisplayGlue.hpp"
#include "Hardware/RotateDisplay.hpp"
#include "Look/DialogLook.hpp"
#include "Profile/File.hpp"
#include "Profile/Map.hpp"
#include "Profile/Profile.hpp"
#include "Screen/Layout.hpp"
#include "UIGlobals.hpp"
#include "UIActions.hpp"
#include "system/FileUtil.hpp"
#include "system/Process.hpp"
#include "ui/event/KeyCode.hpp"
#include "ui/event/Timer.hpp"
#include "ui/window/Init.hpp"
#include "UtilsSettings.hpp"
#include "LogFile.hpp"

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
  TOUCH_SCREEN,

  TOUCH_CALIBRATION,

};

  static constexpr StaticEnumChoice rotation_list[] = {
    // { DisplayOrientation::DEFAULT,  "default" },
    {DisplayOrientation::LANDSCAPE, "Landscape (0°)"},
    {DisplayOrientation::PORTRAIT, "Portrait (90°)"},
    {DisplayOrientation::REVERSE_LANDSCAPE, "Rev. Landscape (180°)"},
    {DisplayOrientation::REVERSE_PORTRAIT, "rev. Portrait (270°)"},
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
  void RotateDisplay(DisplayOrientation orientation) noexcept;
  void SetEnabled(bool enabled) noexcept;

  unsigned brightness = 0;
  unsigned rotation = 0;

  ContainerWindow* parent;

};

void DisplaySettingsWidget::RotateDisplay(
    DisplayOrientation orientation) noexcept {
  ovdevice.SetRotation(orientation, 1); // internal rotation only
}

void DisplaySettingsWidget::SetEnabled([[maybe_unused]] bool enabled) noexcept {
  SetRowEnabled(TOUCH_CALIBRATION, enabled);
}

void DisplaySettingsWidget::OnModified([[maybe_unused]] DataField &df) noexcept
{
  if (IsDataField(ROTATION, df)) {
    RotateDisplay((DisplayOrientation)((const DataFieldEnum &)df).GetValue());
  } else if (IsDataField(BRIGHTNESS, df)) {
    auto new_brightness = ((const DataFieldInteger &)df).GetValue();
    if (new_brightness != brightness)
        ovdevice.SetBrightness(new_brightness / 10);
  } else if (IsDataField(TOUCH_SCREEN, df)) {
    SetEnabled(((const DataFieldBoolean &)df).GetValue());
  }
}


void
DisplaySettingsWidget::Prepare([[maybe_unused]] ContainerWindow &_parent,
                              [[maybe_unused]] const PixelRect &rc) noexcept
{
  parent = &_parent;
  brightness = ovdevice.brightness * 10;
  ovdevice.rotation = ovdevice.GetRotation();
  rotation = (unsigned)ovdevice.rotation;

  LogFmt("Rotation-Init {}", rotation);

  AddEnum(_("Rotation"), _("Rotation Display OpenVario"), rotation_list,
          (unsigned)ovdevice.rotation, this);

  AddInteger(_("Brightness"), _("Brightness Display OpenVario"), "%d%%",
             "%d%%", 10, 100, 10, brightness, this);

  AddBoolean(_("Touch enabled"), _("Enabling the tTouch Screen"), ovdevice.touch, this);

  auto touchBtn = AddButton(_("Calibrate Touch"), [this]() {
    ContainerWindow::SetExitValue(LAUNCH_TOUCH_CALIBRATE);
    UIActions::SignalShutdown(true);
    return mrOK;
  });  
  touchBtn->SetEnabled(ovdevice.touch);
}

bool 
DisplaySettingsWidget::Save([[maybe_unused]] bool &_changed) noexcept {
  bool changed = false;
  if (SaveValueEnum(ROTATION, ovdevice.rotation)) {
    /* On OpenVario never use another orientation as DEFAULT, because we set 
	 * the correct value directly in the config.uEnv */
    Profile::Set(ProfileKeys::MapOrientation,
                 (unsigned)DisplayOrientation::DEFAULT);
    ovdevice.SetRotation(ovdevice.rotation, 6); // fbcon and config.uEnv
    // Now no restart necessary!
	// UI::TopWindow::SetExitValue(EXIT_NEWSTART);
    // require_restart = 
    changed = true;
  }

  if (SaveValueInteger(BRIGHTNESS, "Brightness", brightness)) {
    ovdevice.SetBrightness(brightness / 10);
    changed = true;
  }

  changed |= SaveValue(TOUCH_SCREEN, "TouchScreen", ovdevice.touch, false);
  _changed = changed;
  return true;
}

bool 
ShowDisplaySettingsWidget(ContainerWindow &parent,
                              const DialogLook &look) noexcept {
  TWidgetDialog<DisplaySettingsWidget> sub_dialog(
      WidgetDialog::Full{}, (UI::SingleWindow &)parent, look,
      "OpenVario Display Settings");
  sub_dialog.SetWidget();
  sub_dialog.AddButton(_("Close"), mrOK);
  return sub_dialog.ShowModal();
}

std::unique_ptr<Widget> 
CreateDisplaySettingsWidget() noexcept {
  return std::make_unique<DisplaySettingsWidget>();
}
