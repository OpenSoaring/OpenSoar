// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InputEvents.hpp"
#include "Dialogs/Error.hpp"
#include "Language/Language.hpp"
#include "Interface.hpp"
#include "ActionInterface.hpp"
#include "Message.hpp"
#include "Profile/Profile.hpp"
#include "Profile/Keys.hpp"
#include "Profile/Settings.hpp"
#include "Profile/Current.hpp"
#include "util/Macros.hpp"
#include "util/EnumCast.hpp"
#include "Units/Units.hpp"
#include "Protection.hpp"
#include "UtilsSettings.hpp"
#include "Task/ProtectedTaskManager.hpp"
#include "Audio/VarioGlue.hpp"
#include "system/Path.hpp"
#include "util/StringCompare.hxx"
#include "Components.hpp"
#include "BackendComponents.hpp"

void
InputEvents::eventSounds(const char *misc)
{
  SoundSettings &settings = CommonInterface::SetUISettings().sound;
 // bool OldEnableSoundVario = EnableSoundVario;
  bool enabled = settings.vario.enabled;
  if (StringIsEqual(misc, "toggle"))
    settings.vario.enabled = !enabled;
  else if (StringIsEqual(misc, "on"))
    settings.vario.enabled = true;
  else if (StringIsEqual(misc, "off"))
    settings.vario.enabled = false;
  else if (StringIsEqual(misc, "quieter")) {
    settings.vario.volume = settings.vario.volume / 2;
    if (settings.vario.volume < 1) // settings.vario.max_volume)
      settings.vario.volume = 1;  // don't switch to off!
    // settings.vario.enabled = false;
  } else if (StringIsEqual(misc, "louder")) {
    if (settings.vario.volume > 0)
      settings.vario.volume *= 2;
    else
      settings.vario.volume = 2;
    if (settings.vario.volume > 100) // settings.vario.max_volume)
      settings.vario.volume = 100;
    // settings.vario.enabled = false;
  } else if (StringIsEqual(misc, "show")) {
    if (enabled)
      Message::AddMessage(_("Vario sounds on"));
    else
      Message::AddMessage(_("Vario sounds off"));
    return;
  }

  AudioVarioGlue::Configure(settings.vario);
  if (settings.vario.enabled != enabled)
     Profile::Set(ProfileKeys::SoundAudioVario, settings.vario.enabled);
}

void
InputEvents::eventSnailTrail(const char *misc)
{
  MapSettings &settings_map = CommonInterface::SetMapSettings();

  if (StringIsEqual(misc, "toggle")) {
    unsigned trail_length = (int)settings_map.trail.length;
    trail_length = (trail_length + 1u) % 4u;
    settings_map.trail.length = (TrailSettings::Length)trail_length;
  } else if (StringIsEqual(misc, "off"))
    settings_map.trail.length = TrailSettings::Length::OFF;
  else if (StringIsEqual(misc, "long"))
    settings_map.trail.length = TrailSettings::Length::LONG;
  else if (StringIsEqual(misc, "short"))
    settings_map.trail.length = TrailSettings::Length::SHORT;
  else if (StringIsEqual(misc, "full"))
    settings_map.trail.length = TrailSettings::Length::FULL;
  else if (StringIsEqual(misc, "show")) {
    switch (settings_map.trail.length) {
    case TrailSettings::Length::OFF:
      Message::AddMessage(_("Snail trail off"));
      break;

    case TrailSettings::Length::LONG:
      Message::AddMessage(_("Long snail trail"));
      break;

    case TrailSettings::Length::SHORT:
      Message::AddMessage(_("Short snail trail"));
      break;

    case TrailSettings::Length::FULL:
      Message::AddMessage(_("Full snail trail"));
      break;
    }
  }

  ActionInterface::SendMapSettings(true);
}

void
InputEvents::eventTerrainTopology(const char *misc)
{
  eventTerrainTopography(misc);
}

// Do JUST Terrain/Topography (toggle any, on/off any, show)
void
InputEvents::eventTerrainTopography(const char *misc)
{
  if (StringIsEqual(misc, "terrain toggle"))
    sub_TerrainTopography(-2);
  else if (StringIsEqual(misc, "topography toggle"))
    sub_TerrainTopography(-3);
  else if (StringIsEqual(misc, "topology toggle"))
    sub_TerrainTopography(-3);
  else if (StringIsEqual(misc, "terrain on"))
    sub_TerrainTopography(3);
  else if (StringIsEqual(misc, "terrain off"))
    sub_TerrainTopography(4);
  else if (StringIsEqual(misc, "topography on"))
    sub_TerrainTopography(1);
  else if (StringIsEqual(misc, "topography off"))
    sub_TerrainTopography(2);
  else if (StringIsEqual(misc, "topology on"))
    sub_TerrainTopography(1);
  else if (StringIsEqual(misc, "topology off"))
    sub_TerrainTopography(2);
  else if (StringIsEqual(misc, "show"))
    sub_TerrainTopography(0);
  else if (StringIsEqual(misc, "toggle"))
    sub_TerrainTopography(-1);

  XCSoarInterface::SendMapSettings(true);
}

// Adjust audio deadband of internal vario sounds
// +: increases deadband
// -: decreases deadband
void
InputEvents::eventAudioDeadband(const char *misc)
{
  SoundSettings &settings = CommonInterface::SetUISettings().sound;

  if (StringIsEqual(misc, "+")) {
    if (settings.sound_deadband >= 40)
      return;

    ++settings.sound_deadband;
  }
  if (StringIsEqual(misc, "-")) {
    if (settings.sound_deadband <= 0)
      return;

    --settings.sound_deadband;
  }

  Profile::Set(ProfileKeys::SoundDeadband, settings.sound_deadband);

  // TODO feature: send to vario if available
}

// Bugs
// Adjusts the degradation of glider performance due to bugs
// up: increases the performance by 10%
// down: decreases the performance by 10%
// max: cleans the aircraft of bugs
// min: selects the worst performance (50%)
// show: shows the current bug degradation
void
InputEvents::eventBugs(const char *misc)
{
  if (!backend_components->protected_task_manager)
    return;

  PolarSettings &settings = CommonInterface::SetComputerSettings().polar;
  auto BUGS = settings.bugs;
  auto oldBugs = BUGS;

  if (StringIsEqual(misc, "up")) {
    BUGS += 1 / 10.;
    if (BUGS > 1)
      BUGS = 1;
  } else if (StringIsEqual(misc, "down")) {
    BUGS -= 1 / 10.;
    if (BUGS < 0.5)
      BUGS = 0.5;
  } else if (StringIsEqual(misc, "max"))
    BUGS = 1;
  else if (StringIsEqual(misc, "min"))
    BUGS = 0.5;
  else if (StringIsEqual(misc, "show")) {
    char Temp[100];
    _stprintf(Temp, "%d", (int)(BUGS * 100));
    Message::AddMessage(_("Bugs performance"), Temp);
  }

  if (BUGS != oldBugs) {
    settings.SetBugs(BUGS);
    backend_components->SetTaskPolar(settings);
  }
}

// Ballast
// Adjusts the ballast setting of the glider
// up: increases ballast by 10%
// down: decreases ballast by 10%
// max: selects 100% ballast
// min: selects 0% ballast
// show: displays a status message indicating the ballast percentage
void
InputEvents::eventBallast(const char *misc)
{
  if (!backend_components->protected_task_manager)
    return;

  auto &settings = CommonInterface::SetComputerSettings().polar;
  GlidePolar &polar = settings.glide_polar_task;
  auto BALLAST = polar.GetBallast();
  auto oldBallast = BALLAST;

  if (StringIsEqual(misc, "up")) {
    BALLAST += 1 / 10.;
    if (BALLAST >= 1)
      BALLAST = 1;
  } else if (StringIsEqual(misc, "down")) {
    BALLAST -= 1 / 10.;
    if (BALLAST < 0)
      BALLAST = 0;
  } else if (StringIsEqual(misc, "max"))
    BALLAST = 1;
  else if (StringIsEqual(misc, "min"))
    BALLAST = 0;
  else if (StringIsEqual(misc, "show")) {
    char Temp[100];
    _stprintf(Temp, "%d", (int)(BALLAST * 100));
    /* xgettext:no-c-format */
    Message::AddMessage(_("Ballast %"), Temp);
  }

  if (BALLAST != oldBallast) {
    polar.SetBallast(BALLAST);
    backend_components->SetTaskPolar(settings);
  }
}

// ProfileLoad
// Loads the profile of the specified filename
void
InputEvents::eventProfileLoad(const char *misc)
{
  if (!StringIsEmpty(misc)) {
    Profile::LoadFile(Path(misc));

    MapFileChanged = true;
    WaypointFileChanged = true;
    AirspaceFileChanged = true;
    AirfieldFileChanged = true;

    // assuming all is ok, we can...
    Profile::Use(Profile::map);
  }
}

// ProfileSave
// Saves the profile to the specified filename
void
InputEvents::eventProfileSave(const char *misc)
{
  if (!StringIsEmpty(misc)) {
      try {
        Profile::SaveFile(Path(misc));
      } catch (...) {
        ShowError(std::current_exception(), _("Failed to save file."));
        return;
      }
  }
}

// AdjustForecastTemperature
// Adjusts the maximum ground temperature used by the convection forecast
// +: increases temperature by one degree celsius
// -: decreases temperature by one degree celsius
// show: Shows a status message with the current forecast temperature
void
InputEvents::eventAdjustForecastTemperature(const char *misc)
{
  if (StringIsEqual(misc, "+"))
    CommonInterface::SetComputerSettings().forecast_temperature += Temperature::FromKelvin(1);
  else if (StringIsEqual(misc, "-"))
    CommonInterface::SetComputerSettings().forecast_temperature -= Temperature::FromKelvin(1);
  else if (StringIsEqual(misc, "show")) {
    auto temperature =
      CommonInterface::GetComputerSettings().forecast_temperature;
    char Temp[100];
    _stprintf(Temp, "%f", temperature.ToUser());
    Message::AddMessage(_("Forecast temperature"), Temp);
  }
}

void
InputEvents::eventDeclutterLabels(const char *misc)
{
  static const char *const msg[] = {
    N_("All"),
    N_("Task & Landables"),
    N_("Task"),
    N_("None"),
    N_("Task & Airfields"),
  };
  static constexpr unsigned int n = ARRAY_SIZE(msg);

  static const char *const actions[n] = {
    "all",
    "task+landables",
    "task",
    "none",
    "task+airfields",
  };

  WaypointRendererSettings::LabelSelection &wls =
    CommonInterface::SetMapSettings().waypoint.label_selection;
  if (StringIsEqual(misc, "toggle")) {
    wls = WaypointRendererSettings::LabelSelection(((unsigned)wls + 1) %  n);
    Profile::Set(ProfileKeys::WaypointLabelSelection, (int)wls);
  } else if (StringIsEqual(misc, "show")) {
    char tbuf[64];
    _stprintf(tbuf, "%s: %s", _("Waypoint labels"),
              gettext(msg[(unsigned)wls]));
    Message::AddMessage(tbuf);
  }
  else {
    for (unsigned int i=0; i<n; i++)
      if (StringIsEqual(misc, actions[i]))
        wls = (WaypointRendererSettings::LabelSelection)i;
  }

  /* save new values to profile */
  Profile::Set(ProfileKeys::WaypointLabelSelection,
               EnumCast<WaypointRendererSettings::LabelSelection>()(wls));

  ActionInterface::SendMapSettings(true);
}

void
InputEvents::eventAirspaceDisplayMode(const char *misc)
{
  AirspaceRendererSettings &settings =
    CommonInterface::SetMapSettings().airspace;

  if (StringIsEqual(misc, "all"))
    settings.altitude_mode = AirspaceDisplayMode::ALLON;
  else if (StringIsEqual(misc, "clip"))
    settings.altitude_mode = AirspaceDisplayMode::CLIP;
  else if (StringIsEqual(misc, "auto"))
    settings.altitude_mode = AirspaceDisplayMode::AUTO;
  else if (StringIsEqual(misc, "below"))
    settings.altitude_mode = AirspaceDisplayMode::ALLBELOW;
  else if (StringIsEqual(misc, "off"))
    settings.altitude_mode = AirspaceDisplayMode::ALLOFF;

  TriggerMapUpdate();
}

void
InputEvents::eventOrientation(const char *misc)
{
  MapSettings &settings_map = CommonInterface::SetMapSettings();

  if (StringIsEqual(misc, "northup")) {
    settings_map.cruise_orientation = MapOrientation::NORTH_UP;
    settings_map.circling_orientation = MapOrientation::NORTH_UP;
  } else if (StringIsEqual(misc, "northcircle")) {
    settings_map.cruise_orientation = MapOrientation::TRACK_UP;
    settings_map.circling_orientation = MapOrientation::NORTH_UP;
  } else if (StringIsEqual(misc, "trackcircle")) {
    settings_map.cruise_orientation = MapOrientation::NORTH_UP;
    settings_map.circling_orientation = MapOrientation::TRACK_UP;
  } else if (StringIsEqual(misc, "trackup")) {
    settings_map.cruise_orientation = MapOrientation::TRACK_UP;
    settings_map.circling_orientation = MapOrientation::TRACK_UP;
  } else if (StringIsEqual(misc, "northtrack")) {
    settings_map.cruise_orientation = MapOrientation::TRACK_UP;
    settings_map.circling_orientation = MapOrientation::TARGET_UP;
  } else if (StringIsEqual(misc, "targetup")) {
    settings_map.cruise_orientation = MapOrientation::TARGET_UP;
    settings_map.circling_orientation = MapOrientation::TARGET_UP;
  }

  ActionInterface::SendMapSettings(true);
}

/* Event_TerrainToplogy Changes
   0       Show
   1       Topography = ON
   2       Topography = OFF
   3       Terrain = ON
   4       Terrain = OFF
   -1      Toggle through 4 stages (off/off, off/on, on/off, on/on)
   -2      Toggle terrain
   -3      Toggle topography
 */

void
InputEvents::sub_TerrainTopography(int vswitch)
{
  MapSettings &settings_map = CommonInterface::SetMapSettings();

  if (vswitch == -1) {
    // toggle through 4 possible options
    char val = 0;

    if (settings_map.topography_enabled)
      val++;
    if (settings_map.terrain.enable)
      val += (char)2;

    val++;
    if (val > 3)
      val = 0;

    settings_map.topography_enabled = ((val & 0x01) == 0x01);
    settings_map.terrain.enable = ((val & 0x02) == 0x02);
  } else if (vswitch == -2)
    // toggle terrain
    settings_map.terrain.enable = !settings_map.terrain.enable;
  else if (vswitch == -3)
    // toggle topography
    settings_map.topography_enabled = !settings_map.topography_enabled;
  else if (vswitch == 1)
    // Turn on topography
    settings_map.topography_enabled = true;
  else if (vswitch == 2)
    // Turn off topography
    settings_map.topography_enabled = false;
  else if (vswitch == 3)
    // Turn on terrain
    settings_map.terrain.enable = true;
  else if (vswitch == 4)
    // Turn off terrain
    settings_map.terrain.enable = false;
  else if (vswitch == 0) {
    // Show terrain/topography
    // ARH Let user know what's happening
    char buf[128];

    if (settings_map.topography_enabled)
      _stprintf(buf, "\r\n%s / ", _("On"));
    else
      _stprintf(buf, "\r\n%s / ", _("Off"));

    strcat(buf, settings_map.terrain.enable
            ? _("On") : _("Off"));

    Message::AddMessage(_("Topography/Terrain"), buf);
    return;
  }

  /* save new values to profile */
  Profile::Set(ProfileKeys::DrawTopography,
               settings_map.topography_enabled);
  Profile::Set(ProfileKeys::DrawTerrain,
               settings_map.terrain.enable);

  XCSoarInterface::SendMapSettings(true);
}
