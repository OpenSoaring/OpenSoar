// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef IS_OPENVARIO
// don't use (and compile) this code outside an OpenVario project!

#include "OpenVario/SystemSettingsWidget.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Look/DialogLook.hpp"

#include "Language/Language.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Form/DataField/Listener.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/DataField/Integer.hpp"
#include "Interface.hpp"
#include "UIGlobals.hpp"
#include "ui/window/SingleWindow.hpp"

#include "Dialogs/Message.hpp"
#include "./LogFile.hpp"
#include "UIActions.hpp"
#include "UtilsSettings.hpp"
#include "ProgramVersion.h"

#include "OpenVario/System/OpenVarioDevice.hpp"
#include "OpenVario/System/WifiDialogOV.hpp"


#include <stdio.h>
#include <tchar.h>

enum ControlIndex {
  FW_VERSION,
  FIRMWARE,
  ENABLED,
  ROTATION,
  BRIGHTNESS,
  SENSORD,
  VARIOD,
  SSH,
  TIMEOUT,
  WIFI_BUTTON,

#ifdef _DEBUG
  INTEGERTEST,
#endif
};


#if 1
#include "UIGlobals.hpp"
// -------------------------------------------
class SystemSettingsWidget final : public RowFormWidget, DataFieldListener {
public:
  SystemSettingsWidget() noexcept : RowFormWidget(UIGlobals::GetDialogLook()) {}

  void SetEnabled(bool enabled) noexcept;

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
  bool CheckChanged(bool &changed) noexcept;


private:
  /* methods from DataFieldListener */
  void OnModified(DataField &df) noexcept override;

  unsigned brightness;
};
#endif
  static constexpr StaticEnumChoice timeout_list[] = {
    { 0,  _T("immediately"), },
    { 1,  _T("1s"), },
    { 3,  _T("3s"), },
    { 5,  _T("5s"), },
    { 10, _T("10s"), },
    { 30, _T("30s"), },
    { 60, _T("1min"), },
    { -1, _T("never"), },
    nullptr
  };

  static constexpr StaticEnumChoice enable_list[] = {
    { SSHStatus::ENABLED,   _T("enabled"), },
    { SSHStatus::DISABLED,  _T("disabled"), },
    { SSHStatus::TEMPORARY, _T("temporary"), },
    nullptr
  };

  static constexpr StaticEnumChoice rotation_list[] = {
  // { DisplayOrientation::DEFAULT,  _T("default") }, 
  { DisplayOrientation::LANDSCAPE, _T("Landscape (0°)") },
  { DisplayOrientation::PORTRAIT, _T("Portrait (90°)") },
  { DisplayOrientation::REVERSE_LANDSCAPE, _T("Rev. Landscape (180°)") },
  { DisplayOrientation::REVERSE_PORTRAIT, _T("rev. Portrait (270°)")},
  nullptr
  };

void
SystemSettingsWidget::SetEnabled([[maybe_unused]] bool enabled) noexcept
{
  // this disabled itself: SetRowEnabled(ENABLED, enabled);
  SetRowEnabled(BRIGHTNESS, enabled);
  SetRowEnabled(TIMEOUT, enabled);
}

void
SystemSettingsWidget::OnModified([[maybe_unused]] DataField &df) noexcept
{
  if (IsDataField(ENABLED, df)) {
    // const DataFieldBoolean &dfb = ;
    SetEnabled(((const DataFieldBoolean &)df).GetValue());
  } else if (IsDataField(ROTATION, df)) {
    // ShowMessageBox(_T("Set Rotation"), _T("Rotation"), MB_OK);
    // ovdevice.SetRotation((DisplayOrientation)((const DataFieldEnum &)df).GetValue());
  } else if (IsDataField(BRIGHTNESS, df)) {
    // const DataFieldInteger &dfi = (const DataFieldInteger &)df;
    // (DataFieldInteger*)df)
    ovdevice.SetBrightness(((const DataFieldInteger &)df).GetValue() / 10);
  }  else if (IsDataField(FIRMWARE, df)) {
    // (DataFieldInteger*)df)
    // ConvertString
    ShowMessageBox(_T("FirmWare-Selection"), _T("??File??"), MB_OKCANCEL);
  } 
}

void
SystemSettingsWidget::Prepare(ContainerWindow &parent,
                            const PixelRect &rc) noexcept
{
  RowFormWidget::Prepare(parent, rc);

  brightness = ovdevice.brightness * 10; 


  AddReadOnly(_("Current FW"), _("Current firmware version of OpenVario"),
#if defined(PROGRAM_VERSION)
              _T(PROGRAM_VERSION));
#else
              _T("7.42.21.3"));
#endif
  //  std::string_view version = PROGRAM_VERSION;
  AddFile(_("OV-Firmware"), _("Current firmware file version of OpenVario"),
          "OVImage", _T("*.img.gz\0"), FileType::IMAGE);  // no callback... , this);
  
  AddBoolean(
      _("Settings Enabled"),
      _("Enable the Settings Page"), ovdevice.enabled, this);

   AddEnum(_("Rotation"), _("Rotation Display OpenVario"),
           rotation_list, (unsigned)ovdevice.rotation, this);

   AddInteger(_("Brightness"),
             _("Brightness Display OpenVario"), _T("%d%%"), _T("%d%%"), 10,
              100, 10, brightness, this);
   AddBoolean(_("SensorD"), _("Enable the SensorD"), ovdevice.sensord, this);
   AddBoolean(_("VarioD"), _("Enable the VarioD"), ovdevice.variod, this);
   AddEnum(_("SSH"), _("Enable the SSH Connection"), enable_list,
           ovdevice.ssh);

   AddEnum(_("Program Timeout"), _("Timeout for Program Start."), timeout_list, ovdevice.timeout);

   AddButton(
       _T("Settings Wifi"), [this]() { 
         ShowWifiDialog();
     });
#ifdef _DEBUG
   AddInteger(_("IntegerTest"),
               _("IntegerTest."), _T("%d"), _T("%d"), 0,
                  99999, 1, ovdevice.iTest);
#endif

   SetEnabled(ovdevice.enabled);
}

bool 
SystemSettingsWidget::CheckChanged([[maybe_unused]] bool &_changed) noexcept
{
   return false;
}

bool
SystemSettingsWidget::Save([[maybe_unused]] bool &_changed) noexcept
{
  bool changed = false;
  changed |= SaveValue(ENABLED, "Enabled", ovdevice.enabled, false);

  if (SaveValue(ENABLED, "Enabled", ovdevice.enabled, false)) {
    ovdevice.settings.insert_or_assign("Enabled",
                          ovdevice.brightness ? "True" : "False");
    changed = true;
  }

  if (SaveValueEnum(TIMEOUT, "Timeout", ovdevice.timeout)) {
    ovdevice.settings.insert_or_assign("Timeout",
      std::to_string(ovdevice.timeout));
    changed = true;
  }

  if (SaveValueInteger(BRIGHTNESS, "Brightness", brightness)) {
    ovdevice.SetBrightness(brightness/10);
    changed = true;
  }

#ifdef _DEBUG
  if (SaveValueInteger(INTEGERTEST, "iTest", ovdevice.iTest)) {
    ovdevice.settings.insert_or_assign(
        "iTest", std::to_string(ovdevice.iTest));
    changed = true;
  }
#endif

#if 0 // TOD(August2111) Only Test
  ovdevice.settings.insert_or_assign("OpenSoarData",
                                     "D:/Data/OpenSoarData");
#endif

  if (changed) {
    WriteConfigFile(ovdevice.settings, ovdevice.GetSettingsConfig());
  }

  if (SaveValueEnum(ROTATION, ovdevice.rotation)) {
    // ovdevice.SetRotation(ovdevice.rotation);
    // restart = true;
    require_restart = changed = true;
  }

  if (SaveValueEnum(SSH, ovdevice.ssh))
    ovdevice.SetSSHStatus((SSHStatus)ovdevice.ssh); 

  if (SaveValue(SENSORD, ovdevice.sensord))
    ovdevice.SetSystemStatus("sensord",  ovdevice.sensord); 

  if (SaveValue(VARIOD, ovdevice.variod)) 
    ovdevice.SetSystemStatus("variod", ovdevice.variod); 

  _changed = changed;
  return true;
}

bool
ShowSystemSettingsWidget(ContainerWindow  &parent,
  const DialogLook &look) noexcept
{
  TWidgetDialog<SystemSettingsWidget> sub_dialog(
      WidgetDialog::Full{}, (UI::SingleWindow &) parent, look,
      _T("OpenVario System Settings"));
  sub_dialog.SetWidget();
  sub_dialog.AddButton(_("Save"), mrOK);
  sub_dialog.AddButton(_("Close"), mrOK);
  return sub_dialog.ShowModal();
}
#endif

std::unique_ptr<Widget>
CreateSystemSettingsWidget() noexcept
{
  return std::make_unique<SystemSettingsWidget>();
}



