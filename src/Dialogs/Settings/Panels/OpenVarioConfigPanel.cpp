// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef IS_OPENVARIO
// don't use (and compile) this code outside an OpenVario project!

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

#include "OpenVario/System/System.hpp"
#include "OpenVario/System/WifiDialogOV.hpp"


#include <stdio.h>


enum ControlIndex {
  FIRMWARE,
  ENABLED,
  BRIGHTNESS,
  TIMEOUT,
  SHELL_BUTTON,
  WIFI_BUTTON,

  INTEGERTEST,
};


class OpenVarioConfigPanel final
  : public RowFormWidget, DataFieldListener {
public:
  OpenVarioConfigPanel() noexcept
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

  void SetEnabled(bool enabled) noexcept;

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;

private:
  /* methods from DataFieldListener */
  void OnModified(DataField &df) noexcept override;

  static constexpr StaticEnumChoice timeout_list[] = {
    { 0,  _T("imm."), },
    { 1,  _T("1s"), },
    { 3,  _T("3s"), },
    { 5,  _T("5s"), },
    { 10, _T("10s"), },
    { 30, _T("30s"), },
    { 60, _T("1min"), },
    { -1, _T("never"), },
    nullptr
  };
};

void
OpenVarioConfigPanel::SetEnabled([[maybe_unused]] bool enabled) noexcept
{
  // this disabled itself: SetRowEnabled(ENABLED, enabled);
  SetRowEnabled(BRIGHTNESS, enabled);
  SetRowEnabled(TIMEOUT, enabled);
}

void
OpenVarioConfigPanel::OnModified([[maybe_unused]] DataField &df) noexcept
{
  if (IsDataField(ENABLED, df)) {
    const DataFieldBoolean &dfb = (const DataFieldBoolean &)df;
    SetEnabled(dfb.GetValue());
  }
}

void
OpenVarioConfigPanel::Prepare(ContainerWindow &parent,
                            const PixelRect &rc) noexcept
{
  RowFormWidget::Prepare(parent, rc);

  const TCHAR version[] = _T(PROGRAM_VERSION);

//   auto version = _T("3.2.20 (hard coded)");
  AddReadOnly(_("OV-Firmware-Version"), _("Current firmware version of OpenVario"), version);
  AddBoolean(
      _("Settings Enabled"),
      _("Enable the Settings Page"), ovdevice.enabled, this);

   AddInteger(_("Brightness"),
             _("Brightness Display OpenVario"), _T("%d"), _T("%d%%"), 10,
              100, 10, ovdevice.brightness);

//    AddInteger(_("Program Timeout"),
//              _("Timeout for Program Start."), _T("%d"), _T("%d"), 0,
//               30, 1, ovdevice.timeout);

   AddEnum(_("Program Timeout"), _("Timeout for Program Start."), timeout_list, ovdevice.timeout);

   // auto Btn_Shell = 
   AddButton(
       _T("Shell"), [this]() { 
         ShowMessageBox(_("Button pressed"), _("OV-Button"),
                        MB_OK | MB_ICONERROR);
     });

   AddButton(
       _T("Settings Wifi"), [this]() { 
         ShowWifiDialog();
     });

   AddInteger(_("IntegerTest"),
               _("IntegerTest."), _T("%d"), _T("%d"), 0,
                  99999, 1, ovdevice.iTest);

  SetEnabled(ovdevice.enabled);
}

bool
OpenVarioConfigPanel::Save([[maybe_unused]] bool &_changed) noexcept
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

    // Profile::SaveFile(ovdevice.GetSettingsConfig());
    // the parent dialog don't need to save this values because we have an own
    // config file ('openvario.cfg'):
    //    _changed |= changed;
  }

  return true;
}

std::unique_ptr<Widget>
CreateOpenVarioConfigPanel() noexcept
{
  return std::make_unique<OpenVarioConfigPanel>();
}

#endif


