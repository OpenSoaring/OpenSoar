// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InfoBoxes/Content/MacCready.hpp"
#include "InfoBoxes/Data.hpp"
#include "InfoBoxes/Panel/Panel.hpp"
#include "InfoBoxes/Panel/MacCreadyEdit.hpp"
#include "InfoBoxes/Panel/MacCreadySetup.hpp"
#include "Interface.hpp"
#include "Units/Units.hpp"
#include "Formatter/Units.hpp"
#include "Formatter/UserUnits.hpp"
#include "Language/Language.hpp"


static void
SetVSpeed(InfoBoxData &data, double value) noexcept
{
  char buffer[32];
  FormatUserVerticalSpeed(value, buffer, false);
  data.SetValue(buffer[0] == '+' ? buffer + 1 : buffer);
  data.SetValueUnit(Units::current.vertical_speed_unit);
}

/*
 * Subpart callback function pointers
 */

static constexpr InfoBoxPanel panels[] = {
  { N_("Edit"), LoadMacCreadyEditPanel },
  { N_("Setup"), LoadMacCreadySetupPanel },
  { nullptr, nullptr }
};

const InfoBoxPanel *
InfoBoxContentMacCready::GetDialogContent() noexcept
{
  return panels;
}

/*
 * Subpart normal operations
 */

void
InfoBoxContentMacCready::Update(InfoBoxData &data) noexcept
{
  const ComputerSettings &settings_computer =
    CommonInterface::GetComputerSettings();

  data.SetTitle(settings_computer.task.auto_mc ? _("MC AUTO") : _("MC MANUAL"));
  data.SetValueColor(settings_computer.task.auto_mc ? (2) : (3));

  SetVSpeed(data, settings_computer.polar.glide_polar_task.GetMC());

  const CommonStats &common_stats = CommonInterface::Calculated().common_stats;
  data.SetCommentFromSpeed(common_stats.V_block, false);
}

void
UpdateInfoBoxBugs(InfoBoxData &data) noexcept {
  if (!CommonInterface::GetComputerSettings().stf_switch.IsValid()) {
    data.SetInvalid();
    data.SetValue("Unknown");
    return;
  }
  
  std::string str = std::to_string(
    uround((1.0 - CommonInterface::GetComputerSettings().polar.bugs) * 100)) + " %";
  data.SetValue(str.data());
}

void
UpdateInfoBoxWaterBallast(InfoBoxData &data) noexcept
{
  const Plane &plane = CommonInterface::GetComputerSettings().plane;
  const auto polar_settings = CommonInterface::SetComputerSettings().polar;
  if (plane.empty_mass <= 0) {
    data.SetInvalid();
    return;
  }
    auto dry_mass = polar_settings.glide_polar_task.GetDryMass();
    auto fraction = polar_settings.glide_polar_task.GetBallast();
    auto overload = (dry_mass + fraction * plane.max_ballast) /
      plane.polar_shape.reference_mass;

    if ((overload < 1.0) || (overload > 1.60))
    {
      data.SetInvalid();
     return;
    }
    auto value = overload * plane.polar_shape.reference_mass;

    value -= dry_mass;
      // CommonInterface::SetComputerSettings().polar.glide_polar_task.GetDryMass();
/*
    auto value2 = polar_settings.glide_polar_task.GetDryMass();
    value2 = fraction * value2;
    value2 = fraction * plane.max_ballast;
//    return SendCmd("BAL,%0.2f", value, env);
    value2 = value2 *
      plane.polar_shape.reference_mass;
      */

  char buffer[0x100];
  FormatMass(buffer, value, Units::GetUserMassUnit(), true);
  // Set Value
  data.SetValue(buffer);  //  (std::to_string((int)round(value)) + "kg").data());
  // Set Comment
  // FormatMass(buffer, polar_settings.glide_polar_task.GetDryMass() + value,
  FormatUserMass(dry_mass + value, buffer, true);
  // FormatMass(buffer, dry_mass + value,  Units::GetUserMassUnit(), true);

  std::string comment = buffer;
  comment += " / ";

  FormatWingLoading(buffer, polar_settings.glide_polar_task.GetWingLoading(), 
    Units::GetUserWingLoadingUnit(), true);

  comment += buffer;  //  "50.0" + Units::GetUserWingLoadingUnit());
  data.SetComment(comment.data());
}


