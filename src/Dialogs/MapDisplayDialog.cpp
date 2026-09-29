// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "MapDisplayDialog.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Dialogs/Settings/Panels/GaugesConfigPanel.hpp"
#include "Dialogs/Settings/Panels/PagesConfigPanel.hpp"
#include "Dialogs/Settings/Panels/SymbolsConfigPanel.hpp"
#include "Dialogs/Settings/Panels/WaypointDisplayConfigPanel.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Form/DataField/Enum.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Profile/Profile.hpp"
#include "Profile/Keys.hpp"
#include "ActionInterface.hpp"
#include "Interface.hpp"
#include "UIGlobals.hpp"
#include "Language/Language.hpp"

enum ControlIndex {
  AIRSPACE,
  AIRSPACE_LABELS,
  WAYPOINT_LABELS,
  TRAIL_LENGTH,
  AUTO_ZOOM,
  DISTANCE_RINGS,
  FLARM_GAUGE,
  THERMAL_ASSISTANT,
  PAGE_SETTINGS,
};

class MapDisplayWidget final : public RowFormWidget {
public:
  MapDisplayWidget() noexcept
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

  /**
   * Apply the values and save them in the profile.
   *
   * @return true if anything was changed
   */
  bool Apply() noexcept;

protected:
  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
};

void
MapDisplayWidget::Prepare(ContainerWindow &parent,
                          const PixelRect &rc) noexcept
{
  RowFormWidget::Prepare(parent, rc);

  const MapSettings &map = CommonInterface::GetMapSettings();
  const UISettings &ui = CommonInterface::GetUISettings();

  AddBoolean(_("Airspace"),
             _("Show the airspaces on the map.  This is not saved: after "
               "a restart the airspaces are shown again."),
             map.airspace.enable);

  AddBoolean(_("Airspace Labels"),
             _("Show the names and limits of the airspaces on the map."),
             map.airspace.label_selection !=
             AirspaceRendererSettings::LabelSelection::NONE);

  AddEnum(_("Waypoint labels"), _("Determines what labels are displayed."),
          wp_selection_list, (unsigned)map.waypoint.label_selection);

  AddEnum(_("Trail length"),
          _("Determines whether and how long a snail trail is drawn behind "
            "the glider."),
          trail_length_list, (unsigned)map.trail.length);

  AddBoolean(_("Auto zoom"),
             _("Zoom the map automatically to show the next waypoint."),
             map.auto_zoom_enabled);

  AddBoolean(C_("Setting", "Distance rings"),
             _("Display distance rings around the aircraft on the map."),
             map.distance_rings_enabled);

  AddBoolean(_("FLARM Radar"),
             _("This enables the display of the FLARM radar gauge. The "
               "track bearing of the target relative to the track bearing "
               "of the aircraft is displayed as an arrow head, and a "
               "triangle pointing up or down shows the relative altitude "
               "of the target relative to you. In all modes, the color of "
               "the target indicates the threat level."),
             ui.traffic.enable_gauge);

  AddEnum(_("Thermal Assistant"),
          _("Enable and select the position of the thermal assistant "
            "when overlayed on the main screen."),
          thermal_assistant_position_list,
          (unsigned)ui.thermal_assistant_position);

  AddButton(_("Page settings"), [](){ ShowPageSettingsDialog(); });
}

bool
MapDisplayWidget::Apply() noexcept
{
  MapSettings &map = CommonInterface::SetMapSettings();
  UISettings &ui = CommonInterface::SetUISettings();
  bool changed = false;

  /* the airspace display is a switch for the moment, as the quick
     menu used to be, so it is not saved */
  const bool airspace = GetValueBoolean(AIRSPACE);
  if (airspace != map.airspace.enable) {
    map.airspace.enable = airspace;
    changed = true;
  }

  const auto airspace_labels = GetValueBoolean(AIRSPACE_LABELS)
    ? AirspaceRendererSettings::LabelSelection::ALL
    : AirspaceRendererSettings::LabelSelection::NONE;
  if (airspace_labels != map.airspace.label_selection) {
    map.airspace.label_selection = airspace_labels;
    Profile::Set(ProfileKeys::AirspaceLabelSelection, (int)airspace_labels);
    changed = true;
  }

  changed |= SaveValueEnum(WAYPOINT_LABELS,
                           ProfileKeys::WaypointLabelSelection,
                           map.waypoint.label_selection);
  changed |= SaveValueEnum(TRAIL_LENGTH, ProfileKeys::SnailTrail,
                           map.trail.length);
  changed |= SaveValue(AUTO_ZOOM, ProfileKeys::AutoZoom,
                       map.auto_zoom_enabled);
  changed |= SaveValue(DISTANCE_RINGS, ProfileKeys::DistanceRingsEnabled,
                       map.distance_rings_enabled);
  changed |= SaveValue(FLARM_GAUGE, ProfileKeys::EnableFLARMGauge,
                       ui.traffic.enable_gauge);
  changed |= SaveValueEnum(THERMAL_ASSISTANT, ProfileKeys::TAPosition,
                           ui.thermal_assistant_position);

  if (changed) {
    Profile::Save();
    ActionInterface::SendMapSettings(true);
  }

  return changed;
}

void
ShowMapDisplayDialog() noexcept
{
  const DialogLook &look = UIGlobals::GetDialogLook();
  TWidgetDialog<MapDisplayWidget>
    dialog(WidgetDialog::Auto{}, UIGlobals::GetMainWindow(), look,
           _("Map Display"));
  dialog.AddButton(_("OK"), mrOK);
  dialog.AddButton(_("Cancel"), mrCancel);
  dialog.SetWidget();

  if (dialog.ShowModal() == mrOK)
    dialog.GetWidget().Apply();
}
