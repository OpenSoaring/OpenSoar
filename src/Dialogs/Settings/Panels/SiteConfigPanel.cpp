// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Profile/Keys.hpp"
#include "Language/Language.hpp"
#include "LocalPath.hpp"
#include "UtilsSettings.hpp"
#include "ConfigPanel.hpp"
#include "SiteConfigPanel.hpp"
#include "Widget/RowFormWidget.hpp"
#include "UIGlobals.hpp"
#include "Waypoint/Patterns.hpp"
#include "system/Path.hpp"

#define RASP_FILE_SETTING 0

enum ControlIndex {
  DataPath,
  MapFile,
  WaypointFile,
  AdditionalWaypointFile,
  WatchedWaypointFile,
  AirspaceFile,
  AdditionalAirspaceFile,
  AirfieldFile,
  FlarmFile,
#if RASP_FILE_SETTING
  RaspFile,
#endif
  FrequenciesFile
};

class SiteConfigPanel final : public RowFormWidget {
  enum Buttons {
    WAYPOINT_EDITOR,
  };

public:
  SiteConfigPanel()
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

public:
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  bool Save(bool &changed) noexcept override;
};

void
SiteConfigPanel::Prepare([[maybe_unused]] ContainerWindow &parent,
  [[maybe_unused]] const PixelRect &rc) noexcept
{
  WndProperty *wp = Add("", 0, true);
  wp->SetText(GetPrimaryDataPath().c_str());
  wp->SetEnabled(false);

  AddFile(_("Map database"),
          _("The name of the file (.xcm) containing terrain, topography, "
            "and optionally waypoints, their details and airspaces."),
          ProfileKeys::MapFile, "*.xcm\0*.lkm\0", FileType::MAP);

  AddFile(_("Waypoints"),
          _("Primary waypoints file.  Supported file types are Cambridge "
            "files (.dat), Zander files (.wpz) or SeeYou files (.cup)."),
          ProfileKeys::WaypointFile, WAYPOINT_FILE_PATTERNS,
          FileType::WAYPOINT);

  AddFile(_("More waypoints"),
          _("Secondary waypoints file.  This may be used to add waypoints "
            "for a competition."),
          ProfileKeys::AdditionalWaypointFile, WAYPOINT_FILE_PATTERNS,
          FileType::WAYPOINT);
  SetExpertRow(AdditionalWaypointFile);

  AddFile(_("Watched waypoints"),
          _("Waypoint file containing special waypoints for which additional "
            "computations like calculation of arrival height in map display "
            "always takes place. Useful for waypoints like known reliable "
            "thermal sources (e.g. powerplants) or mountain passes."),
          ProfileKeys::WatchedWaypointFile, WAYPOINT_FILE_PATTERNS,
          FileType::WAYPOINT);
  SetExpertRow(WatchedWaypointFile);

  AddFile(_("Airspaces"), _("The file name of the primary airspace file."),
          ProfileKeys::AirspaceFile, "*.txt\0*.air\0*.sua\0",
          FileType::AIRSPACE);

  AddFile(_("More airspaces"),
          _("The file name of the secondary airspace file."),
          ProfileKeys::AdditionalAirspaceFile, "*.txt\0*.air\0*.sua\0",
          FileType::AIRSPACE);
  SetExpertRow(AdditionalAirspaceFile);

  AddFile(_("Waypoint details"),
          _("The file may contain extracts from enroute supplements or other "
            "contributed information about individual "
            "waypoints and airfields."),
          ProfileKeys::AirfieldFile, "*.txt\0",
          FileType::WAYPOINTDETAILS);
  SetExpertRow(AirfieldFile);

  AddFile(_("FLARM Device Database"),
          _("The name of the file containing information about registered "
            "FLARM devices."),
          ProfileKeys::FlarmFile, "*.fln\0",
          FileType::FLARMNET);

#if RASP_FILE_SETTING
  /* TODO(August2111) : remove RASP setting - personally I cannot see any input
   * on weather page */
  AddFile("RASP", nullptr,
          ProfileKeys::RaspFile, "*-rasp*.dat\0",
          FileType::RASP);
#endif
  AddFile(_("Radio Frequency Database"),
          _("Radio frequencies file."),
          ProfileKeys::FrequenciesFile, "*.xcf\0",
          FileType::FREQUENCIES);
}

bool
SiteConfigPanel::Save(bool &_changed) noexcept
{
  bool changed = false;

  MapFileChanged = SaveValueFileReader(MapFile, ProfileKeys::MapFile);

  // WaypointFileChanged has already a meaningful value
  WaypointFileChanged |= SaveValueFileReader(WaypointFile,
    ProfileKeys::WaypointFile);
  WaypointFileChanged |= SaveValueFileReader(AdditionalWaypointFile,
    ProfileKeys::AdditionalWaypointFile);
  WaypointFileChanged |= SaveValueFileReader(WatchedWaypointFile,
    ProfileKeys::WatchedWaypointFile);

  AirspaceFileChanged = SaveValueFileReader(AirspaceFile,
    ProfileKeys::AirspaceFile);
  AirspaceFileChanged |= SaveValueFileReader(AdditionalAirspaceFile,
    ProfileKeys::AdditionalAirspaceFile);

  FlarmFileChanged = SaveValueFileReader(FlarmFile,
    ProfileKeys::FlarmFile);
  AirfieldFileChanged = SaveValueFileReader(AirfieldFile,
    ProfileKeys::AirfieldFile);

  FrequenciesFileChanged = SaveValueFileReader(FrequenciesFile, ProfileKeys::FrequenciesFile);
#if RASP_FILE_SETTING
  RaspFileChanged = SaveValueFileReader(RaspFile, ProfileKeys::RaspFile);
#endif

  changed = WaypointFileChanged || AirfieldFileChanged || MapFileChanged ||
    FlarmFileChanged
    || FrequenciesFileChanged;
#if  RASP_FILE_SETTING
    || RaspFileChanged
#endif
  ;

  _changed |= changed;

  return true;
}

std::unique_ptr<Widget>
CreateSiteConfigPanel()
{
  return std::make_unique<SiteConfigPanel>();
}
