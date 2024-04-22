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
#include "ProgramVersion.h"

#include "OpenVario/System/OpenVarioDevice.hpp"
#include "OpenVario/System/OpenVarioTools.hpp"
#include "OpenVario/System/WifiDialogOV.hpp"


#include <stdio.h>
#include <tchar.h>

enum ControlIndex {
  FW_VERSION,
  FIRMWARE,
  ENABLED,
  SENSORD,
  VARIOD,
  SSH,
  TIMEOUT,
  WIFI_BUTTON,
  SENSOR_CAL,

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
};
#endif

#ifdef OPENVARIOBASEMENU
  static constexpr StaticEnumChoice timeout_list[] = {
    { 0,  "immediately", },
    { 1,  "1s", },
    { 3,  "3s", },
    { 5,  "5s", },
    { 10, "10s", },
    { 30, "30s", },
    { 60, "1min", },
    { -1, "never", },
    nullptr
  };
#endif

  static constexpr StaticEnumChoice enable_list[] = {
    { SSHStatus::ENABLED,   "enabled", },
    { SSHStatus::DISABLED,  "disabled", },
    { SSHStatus::TEMPORARY, "temporary", },
    nullptr
  };

void
SystemSettingsWidget::SetEnabled([[maybe_unused]] bool enabled) noexcept
{
  // this disabled itself: SetRowEnabled(ENABLED, enabled);
  // SetRowEnabled(BRIGHTNESS, enabled);
#ifdef OPENVARIOBASEMENU
  SetRowEnabled(TIMEOUT, enabled);
#endif
}

void
SystemSettingsWidget::OnModified([[maybe_unused]] DataField &df) noexcept
{
  if (IsDataField(ENABLED, df)) {
    // const DataFieldBoolean &dfb = ;
    SetEnabled(((const DataFieldBoolean &)df).GetValue());
  }  else if (IsDataField(FIRMWARE, df)) {
    // (DataFieldInteger*)df)
    // ConvertString
    ShowMessageBox("FirmWare-Selection", "??File??", MB_OKCANCEL);
  } 
}

void
SystemSettingsWidget::Prepare(ContainerWindow &parent,
                            const PixelRect &rc) noexcept
{
  RowFormWidget::Prepare(parent, rc);

  AddReadOnly(_("Current OpenSoar"), _("Current firmware version of OpenVario"),
#if defined(PROGRAM_VERSION)
              PROGRAM_VERSION);
#else
              "7.42.21.3");
#endif
  AddFile(_("OV-Firmware"), _("Current firmware file version of OpenVario"),
          "OVImage", "*.img.gz\0", FileType::IMAGE);  // no callback... , this);
  
  AddBoolean(
      _("Settings Enabled"),
      _("Enable the Settings Page"), ovdevice.enabled, this);

   AddBoolean(_("SensorD"), _("Enable the SensorD"), ovdevice.sensord, this);
   AddBoolean(_("VarioD"), _("Enable the VarioD"), ovdevice.variod, this);
   AddEnum(_("SSH"), _("Enable the SSH Connection"), enable_list,
           ovdevice.ssh);

#ifdef OPENVARIOBASEMENU
   AddEnum(_("Program Timeout"), _("Timeout for Program Start."), timeout_list, ovdevice.timeout);
#else
   AddDummy();  // Placeholder for enum enumeration
#endif

   auto btnWifi = AddButton(
       "Settings Wifi", [this]() { 
         ShowWifiDialog();
     });
   btnWifi->SetEnabled(true);  // dependend on availability? Missing: 

   AddButton(_("Calibrate Sensors"), CalibrateSensors);


#ifdef _DEBUG
   AddInteger(_("IntegerTest"),
               _("IntegerTest."), "%d", "%d", 0,
                  99999, 1, ovdevice.iTest);
#endif

#if 1
   AddButton(_("Upgrade Firmware (temp.)"), [this]() {
     ContainerWindow::SetExitValue(START_UPGRADE);
     UIActions::SignalShutdown(false);
     return mrOK; // START_UPGRADE;
   });
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
                          ovdevice.enabled ? "True" : "False");
    changed = true;
  }

#ifdef OPENVARIOBASEMENU
  if (SaveValueEnum(TIMEOUT, "Timeout", ovdevice.timeout)) {
    ovdevice.settings.insert_or_assign("Timeout",
      std::to_string(ovdevice.timeout));
    changed = true;
  }
#endif

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

  if (SaveValueEnum(SSH, ovdevice.ssh))
    ovdevice.SetSSHStatus((SSHStatus)ovdevice.ssh); 

  if (SaveValue(SENSORD, ovdevice.sensord))
    ovdevice.SetSystemStatus("sensord",  ovdevice.sensord); 

  if (SaveValue(VARIOD, ovdevice.variod)) 
    ovdevice.SetSystemStatus("variod", ovdevice.variod); 

  _changed = changed;
  return true;
}

// this has only defined in OpenVarioBaseMenu...
bool
ShowSystemSettingsWidget(ContainerWindow  &parent,
  const DialogLook &look) noexcept
{
  TWidgetDialog<SystemSettingsWidget> sub_dialog(
      WidgetDialog::Full{}, (UI::SingleWindow &) parent, look,
      "OpenVario System Settings");
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


