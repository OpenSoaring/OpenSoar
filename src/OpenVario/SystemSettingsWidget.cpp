// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef IS_OPENVARIO
// don't use (and compile) this code outside an OpenVario project!

#include "OpenVario/SystemSettingsWidget.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Look/DialogLook.hpp"

#include "Language/Language.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Form/DataField/Listener.hpp"
#include "Form/DataField/Enum.hpp"
#include "Interface.hpp"
#include "UIGlobals.hpp"
#include "ui/window/SingleWindow.hpp"

#include "Dialogs/Message.hpp"
#include "./LogFile.hpp"

#include "OpenVario/System/OpenVarioDevice.hpp"
#include "OpenVario/System/WifiDialogOV.hpp"


#include <stdio.h>


enum ControlIndex {
  FIRMWARE,
  ENABLED,
  ROTATION,
  BRIGHTNESS,
  SENSORD,
  VARIOD,
  SSH,
  TIMEOUT,
  SHELL_BUTTON,
  WIFI_BUTTON,

  INTEGERTEST,
};


#if 0
class SystemSettingsWidget final
  : public RowFormWidget, DataFieldListener {
public:
  SystemSettingsWidget() noexcept
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

  void SetEnabled(bool enabled) noexcept;

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;

  int OnShow(const UI::SingleWindow &parent) noexcept;

private:
  /* methods from DataFieldListener */
  void OnModified(DataField &df) noexcept override;
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
    const DataFieldBoolean &dfb = (const DataFieldBoolean &)df;
    SetEnabled(dfb.GetValue());
  }
}

void
SystemSettingsWidget::Prepare(ContainerWindow &parent,
                            const PixelRect &rc) noexcept
{
  RowFormWidget::Prepare(parent, rc);

  ovdevice.sensord = OpenvarioGetSensordStatus();
  ovdevice.variod = OpenvarioGetVariodStatus();
  ovdevice.ssh = (unsigned) OpenvarioGetSSHStatus();
  

  const TCHAR version[] = _T(PROGRAM_VERSION);

//   auto version = _T("3.2.20 (hard coded)");
  AddReadOnly(_("OV-Firmware-Version"), _("Current firmware version of OpenVario"), version);
  AddBoolean(
      _("Settings Enabled"),
      _("Enable the Settings Page"), ovdevice.enabled, this);

   AddInteger(_("Rotation"),
             _("Rotation Display OpenVario"), _T("%d"), _T("%d%%"), 0,
              3, 1, ovdevice.rotation);

   AddInteger(_("Brightness"),
             _("Brightness Display OpenVario"), _T("%d"), _T("%d%%"), 10,
              100, 10, ovdevice.brightness);
   AddBoolean(_("SensorD"), _("Enable the SensorD"), ovdevice.sensord, this);
   AddBoolean(_("VarioD"), _("Enable the VarioD"), ovdevice.variod, this);
   AddEnum(_("SSH"), _("Enable the SSH Connection"), enable_list,
           ovdevice.ssh);

   AddEnum(_("Program Timeout"), _("Timeout for Program Start."), timeout_list, ovdevice.timeout);

   // auto Btn_Shell = 
   AddButton(
       _T("Exit to Shell"), [this]() {
         LogFormat("Exit to Shell");
         exit(111); // without cleaning up????
     });

   AddButton(
       _T("Settings Wifi"), [this]() { 
         ShowWifiDialog();
     });

   AddInteger(_("IntegerTest"),
               _("IntegerTest."), _T("%d"), _T("%d"), 0,
                  99999, 1, ovdevice.iTest);

  AddReadOnly(_("OV-Firmware-Version"),
               _("Current firmware version of OpenVario"), version);

  SetEnabled(ovdevice.enabled);
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

  if (SaveValueInteger(BRIGHTNESS, "Brightness", ovdevice.brightness)) {
    ovdevice.settings.insert_or_assign(
        "Brightness", std::to_string(ovdevice.brightness));
    changed = true;
  }

  if (SaveValueInteger(INTEGERTEST, "iTest",ovdevice.iTest)) {
    ovdevice.settings.insert_or_assign(
        "iTest", std::to_string(ovdevice.iTest));
    changed = true;
  }
#if 0 // TOD(August2111) Only Test
  ovdevice.settings.insert_or_assign("OpenSoarData",
                                     "D:/Data/OpenSoarData");
#endif

  if (changed) {
    WriteConfigFile(ovdevice.settings, ovdevice.GetSettingsConfig());
  }

  if (SaveValueEnum(SSH, ovdevice.ssh))
    OpenvarioSetSSHStatus((SSHStatus) ovdevice.ssh); 

  if (SaveValue(SENSORD, ovdevice.sensord))
    OpenvarioSetSensordStatus(ovdevice.sensord); 

  if (SaveValue(VARIOD, ovdevice.variod)) 
    OpenvarioSetSensordStatus(ovdevice.variod); 


  return true;
}

int 
SystemSettingsWidget::OnShow([[maybe_unused]] const UI::SingleWindow &parent)
                             noexcept {
#if 0
  TWidgetDialog<SystemSettingsWidget> sub_dialog(
      WidgetDialog::Full{}, parent, GetLook(),
      _T("OpenVario System Settings"));
  sub_dialog.SetWidget();
  sub_dialog.AddButton(_("Close"), mrOK);
  return sub_dialog.ShowModal();
#else
  return 0;
#endif
}

std::unique_ptr<Widget>
CreateSystemSettingsWidget() noexcept
{
  return std::make_unique<SystemSettingsWidget>();
}

#endif


