// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "VarioDisplayConfigPanel.hpp"
#include "Gauge/VarioDisplaySettings.hpp"
#include "Profile/Keys.hpp"
#include "Profile/Profile.hpp"
#include "ActionInterface.hpp"
#include "Interface.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Form/DataField/Enum.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Language/Language.hpp"
#include "UIGlobals.hpp"
#include "util/StaticString.hxx"

enum ControlIndex {
  CirclingCenter,
  CirclingInfo1,
  CirclingInfo2,
  CirclingInfo3,
  StraightSpacer,
  StraightCenter,
  StraightInfo1,
  StraightInfo2,
  StraightInfo3,
};

static constexpr StaticEnumChoice center_list[] = {
  { VarioDisplayCenterView::NONE, N_("None") },
  { VarioDisplayCenterView::SINGLE_ARROW, N_("Single Arrow"),
    N_("One wind arrow; the tail shows the angle to the average wind.") },
  { VarioDisplayCenterView::DOUBLE_ARROW, N_("Double Arrow"),
    N_("Two arrows: the wind and the average wind.") },
  { VarioDisplayCenterView::DOTTED_ASSISTANT, N_("Dotted Assistant"),
    N_("The thermal assistant made of dots.") },
  { VarioDisplayCenterView::SPIDER_ASSISTANT, N_("Spider Assistant"),
    N_("The thermal assistant as a spider's web.") },
  nullptr
};

static constexpr StaticEnumChoice line_list[] = {
  { VarioDisplayLineView::NONE, N_("None") },
  { VarioDisplayLineView::AVG_CLIMB_RATE, N_("Avg Climb Rate") },
  { VarioDisplayLineView::BANK_ANGLE, N_("Bank Angle") },
  { VarioDisplayLineView::BATTERY_VOLTAGE, N_("Battery Voltage") },
  { VarioDisplayLineView::CIRCLE_DIAMETER, N_("Circle Diameter") },
  { VarioDisplayLineView::CIRCLE_MAX_MIN, N_("Circle Max-Min") },
  { VarioDisplayLineView::DRIFT_ANGLE, N_("Drift Angle") },
  { VarioDisplayLineView::EQUIVALENT_AIRSPEED, N_("Equivalent Air Speed") },
  { VarioDisplayLineView::FLIGHT_LEVEL, N_("Flight Level") },
  { VarioDisplayLineView::G_LOAD, N_("G-Load") },
  { VarioDisplayLineView::HEADING, N_("Heading") },
  { VarioDisplayLineView::INDICATED_AIRSPEED, N_("Indicated Air Speed") },
  { VarioDisplayLineView::PITCH_ANGLE, N_("Pitch Angle") },
  { VarioDisplayLineView::SPEED_TO_FLY, N_("Speed to Fly") },
  { VarioDisplayLineView::TRUE_AIRSPEED, N_("True Air Speed") },
  { VarioDisplayLineView::TRUE_COURSE, N_("True Course") },
  { VarioDisplayLineView::UTC_TIME, N_("UTC Time") },
  nullptr
};

static constexpr StaticEnumChoice line2_list[] = {
  { VarioDisplayLineView::NONE, N_("None") },
  { VarioDisplayLineView::AVG_CLIMB_RATE, N_("Avg Climb Rate") },
  { VarioDisplayLineView::BANK_ANGLE, N_("Bank Angle") },
  { VarioDisplayLineView::BATTERY_VOLTAGE, N_("Battery Voltage") },
  { VarioDisplayLineView::CIRCLE_DIAMETER, N_("Circle Diameter") },
  { VarioDisplayLineView::CIRCLE_MAX_MIN, N_("Circle Max-Min") },
  { VarioDisplayLineView::DRIFT_ANGLE, N_("Drift Angle") },
  { VarioDisplayLineView::EQUIVALENT_AIRSPEED, N_("Equivalent Air Speed") },
  { VarioDisplayLineView::FLIGHT_LEVEL, N_("Flight Level") },
  { VarioDisplayLineView::G_LOAD, N_("G-Load") },
  { VarioDisplayLineView::HEADING, N_("Heading") },
  { VarioDisplayLineView::INDICATED_AIRSPEED, N_("Indicated Air Speed") },
  { VarioDisplayLineView::PITCH_ANGLE, N_("Pitch Angle") },
  { VarioDisplayLineView::SPEED_TO_FLY, N_("Speed to Fly") },
  { VarioDisplayLineView::TRUE_AIRSPEED, N_("True Air Speed") },
  { VarioDisplayLineView::TRUE_COURSE, N_("True Course") },
  { VarioDisplayLineView::UTC_TIME, N_("UTC Time") },
  { VarioDisplayLineView::WIND_AND_AVG_WIND, N_("Wind, avg Wind"),
    N_("The wind and, in a second line, the average wind.") },
  { VarioDisplayLineView::WIND_AND_DELTA, N_("Wind and Delta"),
    N_("The wind and, in a second line, the difference to the average wind.") },
  nullptr
};

static constexpr StaticEnumChoice info3_list[] = {
  { VarioDisplayInfo3View::NONE, N_("None") },
  { VarioDisplayInfo3View::CLIMBING, N_("Climbing"),
    N_("The climb rate averaged over the whole thermal.") },
  { VarioDisplayInfo3View::SPEED_TO_FLY, N_("Speed to Fly") },
  nullptr
};

class VarioDisplayConfigPanel final : public RowFormWidget {
public:
  VarioDisplayConfigPanel()
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
};

void
VarioDisplayConfigPanel::Prepare([[maybe_unused]] ContainerWindow &parent,
                                 [[maybe_unused]] const PixelRect &rc) noexcept
{
  const VarioDisplaySettings &settings =
    CommonInterface::GetUISettings().vario_display;

  AddEnum(_("Circling: centre"),
          _("What the centre of the vario display page shows while circling."),
          center_list, (unsigned)settings.circling.center);
  AddEnum(_("Circling: top row"),
          _("The top info row of the vario display page while circling."),
          line_list, (unsigned)settings.circling.info1);
  AddEnum(_("Circling: bottom row"),
          _("The bottom info row of the vario display page while circling."),
          line2_list, (unsigned)settings.circling.info2);
  AddEnum(_("Circling: right margin"),
          _("The right hand read-out of the vario display page while circling."),
          info3_list, (unsigned)settings.circling.info3);

  AddSpacer();

  AddEnum(_("Straight: centre"),
          _("What the centre of the vario display page shows in straight flight."),
          center_list, (unsigned)settings.straight.center);
  AddEnum(_("Straight: top row"),
          _("The top info row of the vario display page in straight flight."),
          line_list, (unsigned)settings.straight.info1);
  AddEnum(_("Straight: bottom row"),
          _("The bottom info row of the vario display page in straight flight."),
          line2_list, (unsigned)settings.straight.info2);
  AddEnum(_("Straight: right margin"),
          _("The right hand read-out of the vario display page in straight flight."),
          info3_list, (unsigned)settings.straight.info3);
}

bool
VarioDisplayConfigPanel::Save(bool &_changed) noexcept
{
  bool changed = false;

  VarioDisplaySettings &settings =
    CommonInterface::SetUISettings().vario_display;

  changed |= SaveValueEnum(CirclingCenter,
                           ProfileKeys::VarioDisplayCirclingCenter,
                           settings.circling.center);
  changed |= SaveValueEnum(CirclingInfo1,
                           ProfileKeys::VarioDisplayCirclingInfo1,
                           settings.circling.info1);
  changed |= SaveValueEnum(CirclingInfo2,
                           ProfileKeys::VarioDisplayCirclingInfo2,
                           settings.circling.info2);
  changed |= SaveValueEnum(CirclingInfo3,
                           ProfileKeys::VarioDisplayCirclingInfo3,
                           settings.circling.info3);
  changed |= SaveValueEnum(StraightCenter,
                           ProfileKeys::VarioDisplayStraightCenter,
                           settings.straight.center);
  changed |= SaveValueEnum(StraightInfo1,
                           ProfileKeys::VarioDisplayStraightInfo1,
                           settings.straight.info1);
  changed |= SaveValueEnum(StraightInfo2,
                           ProfileKeys::VarioDisplayStraightInfo2,
                           settings.straight.info2);
  changed |= SaveValueEnum(StraightInfo3,
                           ProfileKeys::VarioDisplayStraightInfo3,
                           settings.straight.info3);

  _changed |= changed;
  return true;
}

std::unique_ptr<Widget>
CreateVarioDisplayConfigPanel()
{
  return std::make_unique<VarioDisplayConfigPanel>();
}

void
ShowVarioDisplayConfigDialog()
{
  /* say which of the two view sets is steering the page right now,
     so nobody edits the circling views while looking at straight
     flight */
  StaticString<64> caption;
  caption.Format("%s (%s)", _("Vario Display"),
                 CommonInterface::Calculated().circling
                 ? _("circling") : _("straight flight"));

  VarioDisplayConfigPanel *panel = new VarioDisplayConfigPanel();
  WidgetDialog dialog(WidgetDialog::Auto{}, UIGlobals::GetMainWindow(),
                      UIGlobals::GetDialogLook(), caption, panel);
  dialog.AddButton(_("OK"), mrOK);
  dialog.AddButton(_("Cancel"), mrCancel);

  if (dialog.ShowModal() == mrOK) {
    bool changed = false;
    panel->Save(changed);
    if (changed) {
      Profile::Save();
      /* redraw and tell the listeners - the page picks the new
         views up through OnUISettingsUpdate() */
      ActionInterface::SendMapSettings(true);
    }
  }
}
