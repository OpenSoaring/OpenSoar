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

#ifdef HAVE_RASP
# define RASP_FILE_SETTING 0
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
                   ProfileKeys::AirfieldFileList, _T("*.txt\0"),
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

#ifdef HAVE_RASP
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
<<<<<<< HEAD
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
#ifdef RASP_FILE_SETTING
=======
  WaypointFileChanged |= SaveValueMultiFileReader(
      WaypointFileList, ProfileKeys::WaypointFileList);
  WaypointFileChanged |= SaveValueMultiFileReader(
      WatchedWaypointFileList, ProfileKeys::WatchedWaypointFileList);

  AirspaceFileChanged |= SaveValueMultiFileReader(
      AirspaceFileList, ProfileKeys::AirspaceFileList);

  FlarmFileChanged = SaveValueFileReader(FlarmFile, ProfileKeys::FlarmFile);

  AirfieldFileChanged = SaveValueMultiFileReader(
      AirfieldFileList, ProfileKeys::AirfieldFileList);

>>>>>>> 76f7b7150df1d6ad9e4b93e6e54747b8df8a0ec9
  RaspFileChanged = SaveValueFileReader(RaspFile, ProfileKeys::RaspFile);
#endif

<<<<<<< HEAD
  changed = WaypointFileChanged || AirfieldFileChanged || MapFileChanged ||
    FlarmFileChanged
    || FrequenciesFileChanged;
#ifdef  RASP_FILE_SETTING
    || RaspFileChanged
#endif
  ;
=======
  changed = WaypointFileChanged || AirfieldFileChanged ||
            AirspaceFileChanged || MapFileChanged || FlarmFileChanged ||
            RaspFileChanged;
>>>>>>> 76f7b7150df1d6ad9e4b93e6e54747b8df8a0ec9

  _changed |= changed;

  return true;
}

std::unique_ptr<Widget>
CreateSiteConfigPanel()
{
  return std::make_unique<SiteConfigPanel>();
}
