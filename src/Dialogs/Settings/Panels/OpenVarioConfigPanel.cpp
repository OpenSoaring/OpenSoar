// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef IS_OPENVARIO
// don't use (and compile) this code outside an OpenVario project!

#include "Profile/Keys.hpp"
#include "Language/Language.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Form/DataField/Listener.hpp"
#include "Interface.hpp"
#include "UIGlobals.hpp"

#include "Dialogs/Message.hpp"

#include "OpenVario/System/WifiDialogOV.hpp"


#include <stdio.h>


bool bTest = true;
unsigned iTest = 0;
unsigned iBrightness = 80;

// #define HAVE_WEGLIDE_PILOTNAME
enum ControlIndex {
  OVFirmware,
  OVBooleanTest,
  OVIntegerTest,
  OVBrightness,
  OVButtonShell,
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
};

void
OpenVarioConfigPanel::SetEnabled([[maybe_unused]] bool enabled) noexcept
{
#ifdef OPENVARIO_CONFIG
  // out commented currently:
  SetRowEnabled(WeGlideAutomaticUpload, enabled);
  SetRowEnabled(WeGlidePilotBirthDate, enabled);
  SetRowEnabled(WeGlidePilotID, enabled);
#endif
  // this disabled itself: SetRowEnabled(OVBooleanTest, enabled);
  SetRowEnabled(OVIntegerTest, enabled);
  SetRowEnabled(OVBrightness, enabled);
}

void
OpenVarioConfigPanel::OnModified([[maybe_unused]] DataField &df) noexcept
{
#ifdef OPENVARIO_CONFIG
// out commented currently:
  if (IsDataField(WeGlideEnabled, df)) {
    const DataFieldBoolean &dfb = (const DataFieldBoolean &)df;
    SetEnabled(dfb.GetValue());
  }
#endif
  if (IsDataField(OVBooleanTest, df)) {
    const DataFieldBoolean &dfb = (const DataFieldBoolean &)df;
    SetEnabled(dfb.GetValue());
  }
}

void
OpenVarioConfigPanel::Prepare(ContainerWindow &parent,
                            const PixelRect &rc) noexcept
{
  // const WeGlideSettings &weglide = CommonInterface::GetComputerSettings().weglide;

  RowFormWidget::Prepare(parent, rc);

  // void AddReadOnly(label, help,text;
  auto version = _T("3.2.20");
  AddReadOnly(_("OV-Firmware-Version"), _("Current firmware version of OpenVario"), version);
  AddBoolean(
      _("Boolean Test"),
      _("Boolean Test."),
      bTest, this);

   AddInteger(_("Integer Test"),
             _("Integer Test."),
             _T("%d"), _T("%d"), 1, 99999, 1, iTest);

   AddInteger(_("Brightness Test"),
             _("Brightness ???."), _T("%d"), _T("%d%%"), 10,
              100, 10, iBrightness);

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


  SetEnabled(bTest);
}

bool
OpenVarioConfigPanel::Save([[maybe_unused]] bool &_changed) noexcept
{
  bool changed = false;
#ifdef OPENVARIO_CONFIG
  // out commented currently:

  auto &weglide = CommonInterface::SetComputerSettings().weglide;

  changed |= SaveValue(WeGlideAutomaticUpload,
                       ProfileKeys::WeGlideAutomaticUpload,
                       weglide.automatic_upload);

  changed |= SaveValueInteger(WeGlidePilotID, ProfileKeys::WeGlidePilotID,
                              weglide.pilot_id);

  changed |= SaveValue(WeGlidePilotBirthDate,
                       ProfileKeys::WeGlidePilotBirthDate,
                       weglide.pilot_birthdate);

  changed |= SaveValue(WeGlideEnabled, ProfileKeys::WeGlideEnabled,
                       weglide.enabled);

 
  #endif
  changed |= SaveValue(OVBooleanTest, "OVBooleanTest",
                              bTest, this);

  changed |= SaveValueInteger(OVIntegerTest, "OVIntegerTest", iTest);

  changed |= SaveValueInteger(OVBrightness, "OVBrightness", iBrightness);

  _changed |= changed;
  return true;
}

std::unique_ptr<Widget>
CreateOpenVarioConfigPanel() noexcept
{
  return std::make_unique<OpenVarioConfigPanel>();
}
#endif