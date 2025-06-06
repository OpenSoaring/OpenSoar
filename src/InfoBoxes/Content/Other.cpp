// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InfoBoxes/Content/Other.hpp"
#include "InfoBoxes/Data.hpp"
#include "InfoBoxes/Panel/Panel.hpp"
#include "Interface.hpp"
#include "Renderer/HorizonRenderer.hpp"
#include "Hardware/PowerGlobal.hpp"
#include "system/SystemLoad.hpp"
#include "Language/Language.hpp"
#include "UIGlobals.hpp"
#include "Look/Look.hpp"
#include "Interface.hpp"
#include "UIState.hpp"

#ifdef HAVE_BATTERY
#include "Hardware/PowerInfo.hpp"
#endif

const InfoBoxPanel infobox_panel[] = {
   { nullptr, nullptr }
};

void
UpdateInfoBoxHeartRate(InfoBoxData &data) noexcept
{
  const auto &basic = CommonInterface::Basic();

  if (!basic.heart_rate_available) {
    data.SetInvalid();
    return;
  }

  data.FmtValue("{}", basic.heart_rate);
}

void
UpdateInfoBoxGLoad(InfoBoxData &data) noexcept
{
  if (!CommonInterface::Basic().acceleration.available) {
    data.SetInvalid();
    return;
  }

  // Set Value
  data.FmtValue("{:2.2f}", CommonInterface::Basic().acceleration.g_load);
}

void
UpdateInfoBoxBattery(InfoBoxData &data) noexcept
{
#ifdef HAVE_BATTERY
  const auto &info = Power::global_info;
  const auto &battery = info.battery;
  const auto &external = info.external;

  bool DisplaySupplyVoltageAsValue=false;
  switch (external.status) {
  case Power::ExternalInfo::Status::OFF:
    if (CommonInterface::Basic().battery_level_available)
      data.FmtComment("{}; {}%",
                      _("AC Off"),
                      (int)CommonInterface::Basic().battery_level);
    else
      data.SetComment(_("AC Off"));
    break;

  case Power::ExternalInfo::Status::ON:
    if (!CommonInterface::Basic().voltage_available)
      data.SetComment(_("AC ON"));
    else{
      DisplaySupplyVoltageAsValue = true;
      data.SetValueFromVoltage(CommonInterface::Basic().voltage);
    }
    break;

  case Power::ExternalInfo::Status::UNKNOWN:
  default:
    data.SetCommentInvalid();
  }

  if (battery.remaining_percent) {
    if (!DisplaySupplyVoltageAsValue)
      data.SetValueFromPercent(*battery.remaining_percent);
    else
      data.SetCommentFromPercent(* battery.remaining_percent);
  } else {
    if (!DisplaySupplyVoltageAsValue)
      data.SetValueInvalid();
    else
      data.SetCommentInvalid();
  }

  return;

#endif

  if (CommonInterface::Basic().voltage_available) {
    data.SetValueFromVoltage(CommonInterface::Basic().voltage);
    return;
  } else if (CommonInterface::Basic().battery_level_available) {
    data.SetValueFromPercent(CommonInterface::Basic().battery_level);
    return;
  }

  data.SetInvalid();
}

void
UpdateInfoBoxExperimental1(InfoBoxData &data) noexcept
{
  // Set Value
  data.SetInvalid();
}

void
UpdateInfoBoxExperimental2(InfoBoxData &data) noexcept
{
  // Set Value
  data.SetInvalid();
}

void
UpdateInfoBoxCPULoad(InfoBoxData &data) noexcept
{
  const auto percent_load = SystemLoadCPU();
  if (percent_load) {
    data.SetValueFromPercent(*percent_load);
  } else {
    data.SetInvalid();
  }
}

void
UpdateInfoBoxFreeRAM(InfoBoxData &data) noexcept
{
  // used to be implemented on WinCE
  data.SetInvalid();
}

const InfoBoxPanel *
InfoBoxContentHorizon::GetDialogContent() noexcept
{
  return infobox_panel;  // the general empty InfoBoxSet
}

void
InfoBoxContentHorizon::OnCustomPaint(Canvas &canvas,
                                     const PixelRect &rc) noexcept
{
  if (CommonInterface::Basic().acceleration.available) {
    const Look &look = UIGlobals::GetLook();
    HorizonRenderer::Draw(canvas, rc,
                          look.horizon, CommonInterface::Basic().attitude);
  }
}

void
InfoBoxContentHorizon::Update(InfoBoxData &data) noexcept
{
  const auto &basic = CommonInterface::Basic();

  if (!basic.attitude.bank_angle_available &&
      !basic.attitude.pitch_angle_available) {
    data.SetInvalid();
    return;
  }

  data.SetCustom(basic.attitude.bank_angle_available.ToInteger() +
                 basic.attitude.pitch_angle_available.ToInteger());
}

// TODO: merge with original copy from Dialogs/StatusPanels/SystemStatusPanel.cpp
[[gnu::pure]]
static const char *
GetGPSStatus(const NMEAInfo &basic) noexcept
{
  if (!basic.alive)
    return N_("Disconnected");
  else if (!basic.location_available)
    return N_("Fix invalid");
  else if (!basic.gps_altitude_available)
    return N_("2D fix");
  else
    return N_("3D fix");
}

void
UpdateInfoBoxNbrSat(InfoBoxData &data) noexcept
{
    const NMEAInfo &basic = CommonInterface::Basic();
    const GPSState &gps = basic.gps;

    data.SetComment(gettext(GetGPSStatus(basic)));

    if (!basic.alive)
        data.SetComment(_("No GPS"));
    else if (gps.satellites_used_available) {
        // known number of sats
        data.FmtValue("{}", gps.satellites_used);
    } else {
        // valid but unknown number of sats
        data.SetValueInvalid();
    }
}

void
UpdateInfoBoxPageIndex(InfoBoxData &data) noexcept
{
  const PageSettings &settings = CommonInterface::GetUISettings().pages;
  const PagesState &state = CommonInterface::GetUIState().pages;

  //  data.SetComment(gettext("Page No"));

  switch (settings.pages[state.current_index].main) {
    case PageLayout::Main::MAP:
      data.SetComment(_("Map"));
      break;

    case PageLayout::Main::FLARM_RADAR:
      data.SetComment(_("FLARM radar"));
      break;

    case PageLayout::Main::THERMAL_ASSISTANT:
      data.SetComment(_("Thermal assistant"));
      break;

    case PageLayout::Main::HORIZON:
      data.SetComment(_("Horizon"));
      break;

    default:
      data.SetComment(_("unknown"));
      break;

  }

  data.FmtValue("{} / {}", state.current_index + 1, settings.n_pages);

}
