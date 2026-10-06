// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InfoBoxes/Content/OpenSoar.hpp"
#include "InfoBoxes/Data.hpp"
#include "Interface.hpp"
#include "Units/Units.hpp"
#include "Language/Language.hpp"
#include "UIState.hpp"
#include "PageSettings.hpp"
#include "PageState.hpp"
#include "util/StaticString.hxx"

#include <span>

/*
 * wind
 */

/**
 * Speed as value (with the wind unit), bearing as comment - the
 * layout of the upstream wind boxes.
 */
static void
SetWind(InfoBoxData &data, const SpeedVector &wind) noexcept
{
  data.FmtValue("{:2.0f}", Units::ToUserWindSpeed(wind.norm));
  data.SetValueUnit(Units::current.wind_speed_unit);
  data.SetComment(wind.bearing);
}

void
UpdateInfoBoxInstantaneousWindSpeed(InfoBoxData &data) noexcept
{
  const NMEAInfo &basic = CommonInterface::Basic();
  if (!basic.external_instantaneous_wind_available) {
    data.SetInvalid();
    return;
  }

  SetWind(data, basic.external_instantaneous_wind);
}

void
UpdateInfoBoxInstantaneousWindBearing(InfoBoxData &data) noexcept
{
  const NMEAInfo &basic = CommonInterface::Basic();
  if (!basic.external_instantaneous_wind_available) {
    data.SetInvalid();
    return;
  }

  const SpeedVector &wind = basic.external_instantaneous_wind;
  data.SetValue(wind.bearing);
  data.FmtComment("{:2.0f} {}", Units::ToUserWindSpeed(wind.norm),
                  Units::GetWindSpeedName());
}

void
UpdateInfoBoxInternalWind(InfoBoxData &data) noexcept
{
  /* XCSoar's own estimate, whatever the effective wind is set to */
  const DerivedInfo &calculated = CommonInterface::Calculated();
  if (!calculated.estimated_wind_available) {
    data.SetInvalid();
    return;
  }

  SetWind(data, calculated.estimated_wind);
}

void
UpdateInfoBoxInternalZigZagWind(InfoBoxData &data) noexcept
{
  /* the EKF ("zig-zag") estimator is not exposed on its own; it is
     the one in charge while the effective wind comes from it */
  const DerivedInfo &calculated = CommonInterface::Calculated();
  if (!calculated.estimated_wind_available ||
      calculated.wind_source != DerivedInfo::WindSource::EKF) {
    data.SetInvalid();
    return;
  }

  SetWind(data, calculated.estimated_wind);
}

/*
 * settings and switches
 */

void
UpdateInfoBoxSTFSwitch(InfoBoxData &data) noexcept
{
  /* the switch state a connected vario reports (Vega and others);
     until OpenSoar's own STF/vario flag (flaps, RemoteStick) is back,
     this is the only source */
  const auto &state = CommonInterface::Basic().switch_state;

  switch (state.flight_mode) {
  case SwitchState::FlightMode::CRUISE:
    data.SetValue(_("STF"));
    break;

  case SwitchState::FlightMode::CIRCLING:
    data.SetValue(_("Vario"));
    break;

  case SwitchState::FlightMode::UNKNOWN:
    data.SetInvalid();
    return;
  }

  data.SetCommentInvalid();
}

void
UpdateInfoBoxBugsSetting(InfoBoxData &data) noexcept
{
  /* the same figure the Flight Setup dialog shows: 100 % = clean */
  const auto &polar = CommonInterface::GetComputerSettings().polar;
  data.SetValueFromPercent(polar.bugs * 100);
  data.SetCommentInvalid();
}

void
UpdateInfoBoxTrueHeading(InfoBoxData &data) noexcept
{
  const NMEAInfo &basic = CommonInterface::Basic();
  if (!basic.attitude.heading_available) {
    data.SetInvalid();
    return;
  }

  data.SetValue(basic.attitude.heading);
  data.SetCommentInvalid();
}

void
UpdateInfoBoxWaterBallast(InfoBoxData &data) noexcept
{
  const auto &settings = CommonInterface::GetComputerSettings();
  const double max_ballast = settings.plane.max_ballast;
  if (max_ballast <= 0) {
    /* a plane without water: nothing to show */
    data.SetInvalid();
    return;
  }

  const double litres = settings.polar.glide_polar_task.GetBallastLitres();
  /* litres of water: no Unit for that, so the "l" goes into the text */
  data.FmtValue("{:.0f} l", litres);
  data.FmtComment("{:.0f} %", litres * 100 / max_ballast);
}

/*
 * system
 */

void
UpdateInfoBoxPageNo(InfoBoxData &data) noexcept
{
  const PageSettings &settings = CommonInterface::GetUISettings().pages;
  const PagesState &state = CommonInterface::GetUIState().pages;

  if (state.current_index >= PageSettings::MAX_PAGES ||
      state.current_index >= settings.n_pages) {
    data.SetInvalid();
    return;
  }

  /* "3 / 5": where one is among the configured pages, not just a
     bare number */
  data.FmtValue("{} / {}", state.current_index + 1, settings.n_pages);

  StaticString<64> title;
  const PageLayout &page = settings.pages[state.current_index];
  data.SetComment(page.MakeTitle(CommonInterface::GetUISettings().info_boxes,
                                 std::span<char>{title.buffer(), title.capacity()},
                                 nullptr, true));
}
