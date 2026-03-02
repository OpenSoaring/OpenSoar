// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "SiteConfigPanel.hpp"
#include "Airspace/Patterns.hpp"
#include "ConfigPanel.hpp"
#include "Language/Language.hpp"
#include "LocalPath.hpp"
#include "Profile/Keys.hpp"
#include "UIGlobals.hpp"
#include "UtilsSettings.hpp"
#include "Waypoint/Patterns.hpp"
#include "Widget/RowFormWidget.hpp"
#include "system/Path.hpp"
#include "Weather/Features.hpp"

#ifdef HAVE_RASP
# define RASP_FILE_SETTING 1
#endif

enum ControlIndex {
  DataPath,
  MapFile,
  WaypointFileList,
  WatchedWaypointFileList,
  AirfieldFileList,
  AirspaceFileList,
  FlarmFile,
#ifdef RASP_FILE_SETTING
  RaspFile,
#endif
  FrequenciesFile,
  ChecklistFile,
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

  AddMultipleFiles(_("Waypoints"),
                   _("Primary waypoints files.  Supported file types are "
                     "Cambridge/WinPilot files (.dat), "
                     "Zander files (.wpz) or SeeYou files (.cup)."),
                   ProfileKeys::WaypointFileList, WAYPOINT_FILE_PATTERNS,
                   FileType::WAYPOINT);

  AddMultipleFiles(_("Watched waypoints"),
                   _("Waypoint files containing special waypoints for which "
                     "additional computations like "
                     "calculation of arrival height in map display always "
                     "takes place. Useful for "
                     "waypoints like known reliable thermal sources (e.g. "
                     "powerplants) or mountain passes."),
                   ProfileKeys::WatchedWaypointFileList,
                   WAYPOINT_FILE_PATTERNS, FileType::WAYPOINT);
  SetExpertRow(WatchedWaypointFileList);

  AddMultipleFiles(_("Airfields or Waypoint details"),
                   _("The files may contain extracts from enroute supplements "
                     "or other contributed "
                     "information about individual waypoints and airfields."),
                   ProfileKeys::AirfieldFileList, "*.txt\0",
                   FileType::WAYPOINTDETAILS);
  SetExpertRow(AirfieldFileList);

  AddMultipleFiles(_("Selected Airspace Files"),
                   _("List of active airspace files. Use the Add and Remove "
                     "buttons to activate or deactivate"
                     " airspace files respectively. Supported file types are: "
                     "Openair (.txt /.air), and Tim Newport-Pearce (.sua)."),
                   ProfileKeys::AirspaceFileList, AIRSPACE_FILE_PATTERNS,
                   FileType::AIRSPACE);
  AddFile(_("FLARM Device Database"),
          _("The name of the file containing information about registered "
            "FLARM devices."),
          ProfileKeys::FlarmFile, "*.fln\0",
          FileType::FLARMNET);

#ifdef RASP_FILE_SETTING
  /* TODO(August2111) : remove RASP setting - personally I cannot see any input
   * on weather page */
  AddFile("RASP",
          _("Regional Atmospheric Soaring Prediction file providing "
            "weather forecasts for soaring. Displays color-coded map "
            "overlays for thermal strength, boundary layer winds, "
            "cloud cover, and other soaring-relevant parameters at "
            "various forecast times throughout the day."),
          ProfileKeys::RaspFile, "*-RASP*.dat\0",
          FileType::RASP);
#endif
  AddFile(_("Radio Frequency Database"),
          _("Radio frequencies file."),
          ProfileKeys::FrequenciesFile, "*.xcf\0",
          FileType::FREQUENCIES);
  AddFile(_("Checklist"),
          _("The checklist file containing pre-flight and other checklists."),
          ProfileKeys::ChecklistFile, "*.xcc\0xcsoar-checklist.txt\0",
          FileType::CHECKLIST);

}

bool
SiteConfigPanel::Save(bool &_changed) noexcept
{
  bool changed = false;

  MapFileChanged = SaveValueFileReader(MapFile, ProfileKeys::MapFile);

  // WaypointFileChanged has already a meaningful value
  WaypointFileChanged |= SaveValueMultiFileReader(
      WaypointFileList, ProfileKeys::WaypointFileList);
  WaypointFileChanged |= SaveValueMultiFileReader(
      WatchedWaypointFileList, ProfileKeys::WatchedWaypointFileList);

  AirspaceFileChanged |= SaveValueMultiFileReader(
      AirspaceFileList, ProfileKeys::AirspaceFileList);

  FlarmFileChanged = SaveValueFileReader(FlarmFile, ProfileKeys::FlarmFile);

  AirfieldFileChanged = SaveValueMultiFileReader(
      AirfieldFileList, ProfileKeys::AirfieldFileList);
	  
  bool ChecklistFileChanged = SaveValueFileReader(ChecklistFile, ProfileKeys::ChecklistFile);

  FrequenciesFileChanged = SaveValueFileReader(FrequenciesFile, ProfileKeys::FrequenciesFile);

  changed = WaypointFileChanged || AirfieldFileChanged ||
            AirspaceFileChanged || MapFileChanged || FlarmFileChanged || 
            ChecklistFileChanged || FrequenciesFileChanged;
#ifdef  RASP_FILE_SETTING
  RaspFileChanged = SaveValueFileReader(RaspFile, ProfileKeys::RaspFile);
  changed |= RaspFileChanged;
#endif

  _changed |= changed;

  return true;
}

std::unique_ptr<Widget>
CreateSiteConfigPanel()
{
  return std::make_unique<SiteConfigPanel>();
}
