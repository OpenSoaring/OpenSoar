// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InfoBoxes/Content/Factory.hpp"

#include "InfoBoxes/Content/Base.hpp"
#include "InfoBoxes/Content/Alternate.hpp"
#include "InfoBoxes/Content/Altitude.hpp"
#include "InfoBoxes/Content/Direction.hpp"
#include "InfoBoxes/Content/Glide.hpp"
#include "InfoBoxes/Content/MacCready.hpp"
#include "InfoBoxes/Content/Other.hpp"
#include "InfoBoxes/Content/Speed.hpp"
#include "InfoBoxes/Content/Task.hpp"
#include "InfoBoxes/Content/Places.hpp"
#include "InfoBoxes/Content/Contest.hpp"
#include "InfoBoxes/Content/Team.hpp"
#include "InfoBoxes/Content/Terrain.hpp"
#include "InfoBoxes/Content/Thermal.hpp"
#include "InfoBoxes/Content/Time.hpp"
#include "InfoBoxes/Content/Trace.hpp"
#include "InfoBoxes/Content/Weather.hpp"
#include "InfoBoxes/Content/Airspace.hpp"
#include "InfoBoxes/Content/Radio.hpp"
#include "InfoBoxes/Content/Engine.hpp"

#include "util/Macros.hpp"
#include "Language/Language.hpp"

#include <cstddef>
#include <cassert>

/**
 * An #InfoBoxContent implementation that invokes a callback.  This is
 * used for those contents that would implement only the Update()
 * method and need no context.
 */
class InfoBoxContentCallback : public InfoBoxContent {
  void (*update)(InfoBoxData &data) noexcept;
  const InfoBoxPanel *panels;

public:
  InfoBoxContentCallback(void (*_update)(InfoBoxData &data) noexcept,
                         const InfoBoxPanel *_panels) noexcept
    :update(_update), panels(_panels) {}

  void Update(InfoBoxData &data) noexcept override {
    update(data);
  }

  const InfoBoxPanel *GetDialogContent() noexcept override {
    return panels;
  }
};

template<class T>
struct IBFHelper {
  static InfoBoxContent *Create() noexcept {
    return new T();
  }
};

template<class T, int param>
struct IBFHelperInt {
  static InfoBoxContent *Create() noexcept {
    return new T(param);
  }
};

using namespace InfoBoxFactory;

struct MetaData {
  std::string_view group;
  const char *name;
  const char *caption;
  const char *description;
  InfoBoxContent *(*create)() noexcept;
  void (*update)(InfoBoxData &data) noexcept;
  const InfoBoxPanel *panels;

  /**
   * Implicit instances shall not exist.  This declaration ensures at
   * compile time that the meta_data array is not larger than the
   * number of explicitly initialised elements.
   */
  MetaData() = delete;

  constexpr MetaData(
    std::string_view _group,
    const char *_name,
    const char *_caption,
    const char *_description,
    InfoBoxContent *(*_create)() noexcept) noexcept
    :group(_group), name(_name), caption(_caption), description(_description),
     create(_create), update(nullptr), panels(infobox_panel) {}

  constexpr MetaData(std::string_view _group, 
    const char *_name,
                     const char *_caption,
                     const char *_description,
                     void (*_update)(InfoBoxData &data) noexcept) noexcept
    : group(_group), name(_name), caption(_caption), description(_description),
     create(nullptr), update(_update), panels(infobox_panel) {}

  constexpr MetaData(std::string_view _group, const char *_name,
                     const char *_caption,
                     const char *_description,
                     void (*_update)(InfoBoxData &data) noexcept,
                     const InfoBoxPanel _panels[]) noexcept
    :group(_group), name(_name), caption(_caption), description(_description),
     create(nullptr), update(_update), panels(_panels) {}
};

/* WARNING: Never insert or delete items or rearrange the order of the items
 * in this array. This will break existing infobox configurations of all users!
 */

static constexpr MetaData meta_data[] = {
  // 0..9 
  // e_HeightGPS
  {
    "Altitude",
    N_("Altitude GPS"),
    N_("Alt GPS"),
    N_("This is the altitude above mean sea level reported by the GPS. Touch-screen/PC only: In simulation mode, this value is adjustable with the up/down arrow keys and the right/left arrow keys also cause the glider to turn."),
    IBFHelper<InfoBoxContentAltitudeGPS>::Create,
  },

  // e_HeightAGL
  {
    "Altitude",
    N_("Height AGL"),
    N_("H AGL"),
    N_("This is the navigation altitude minus the terrain elevation obtained from the terrain file. The value is coloured red when the glider is below the terrain safety clearance height."),
    UpdateInfoBoxAltitudeAGL,
    altitude_infobox_panels,
  },

  // e_Thermal_30s
  {
    "Thermal",
    N_("Thermal climb, last 30 s"),
    N_("TC 30s"),
    N_("A 30 second rolling average climb rate based of the reported GPS altitude, or vario if available. The number in smaller font reflects the climb rate for the current thermal since circling started."),
    UpdateInfoBoxThermal30s,
  },

  // e_Bearing
  {
    "Directions",
    N_("Next bearing"),
    N_("Bearing"),
    N_("True bearing of the next waypoint.  For AAT tasks, this is the true bearing to the target within the AAT sector."),
    UpdateInfoBoxBearing,
    next_waypoint_infobox_panels,
  },

  // e_GR_Instantaneous
  {
    "GlideComputer",
    N_("GR instantaneous"),
    N_("GR Inst"),
    N_("Instantaneous glide ratio over ground, given by the ground speed divided by the vertical speed (GPS speed) over the last 20 seconds. Negative values indicate climbing cruise. If the vertical speed is close to zero, the displayed value is '---'."),
    UpdateInfoBoxGRInstant,
  },

  // e_GR_Cruise
  {
    "GlideComputer",
    N_("GR cruise"),
    N_("GR Cruise"),
    N_("The distance from the top of the last thermal, divided by the altitude lost since the top of the last thermal. Negative values indicate climbing cruise (height gain since leaving the last thermal). If the vertical speed is close to zero, the displayed value is '---'."),
    UpdateInfoBoxGRCruise,
  },

  // e_Speed_GPS
  {
    "GlideComputer",
    N_("Speed ground"),
    N_("V GND"),
    N_("Ground speed measured by the GPS. The small value shows the head or tail wind difference to TAS for the current vector. If this InfoBox is active in simulation mode, pressing the up and down arrows adjusts the speed, and left and right turn the glider."),
    IBFHelper<InfoBoxContentSpeedGround>::Create,
  },

  // e_TL_Avg
  {
    "Thermal",
    N_("Last thermal average"),
    N_("TL Avg"),
    N_("Total altitude gain/loss in the last thermal divided by the time spent circling."),
    UpdateInfoBoxThermalLastAvg,
  },

  // e_TL_Gain
  {
    "Thermal",
    N_("Last thermal gain"),
    N_("TL Gain"),
    N_("Total altitude gain/loss in the last thermal. The number in smaller font reflects the overall climb rate for the last thermal."),
    UpdateInfoBoxThermalLastGain,
  },

  // e_TL_Time
  {
    "Thermal",
    N_("Last thermal duration"),
    N_("TL duration"),
    N_("Time spent circling in the last thermal."),
    UpdateInfoBoxThermalLastTime,
  },

  // 10..19 
  // e_MacCready
  {
    "Thermal",  // is this ok? Not really a 'Thermal' info
    N_("MacCready setting"),
    N_("MC"),
    N_("The current MacCready setting and the current MacCready mode (manual or auto). (Touch-screen/PC only) Also used to adjust the MacCready setting if the InfoBox is active, by using the up/down cursor keys."),
    IBFHelper<InfoBoxContentMacCready>::Create,
  },

  // e_WP_Distance
  {
    "Waypoint", 
    N_("Next distance"),
    N_("WP Dist"),
    N_("The distance to the currently selected waypoint. For AAT tasks, this is the distance to the target within the AAT sector."),
    UpdateInfoBoxNextDistance,
    next_waypoint_infobox_panels,
  },

  // e_WP_AltDiff
  {
    "Waypoint",
    N_("Next altitude difference"),
    N_("WP AltD"),
    N_("Arrival altitude at the next waypoint relative to the safety arrival height. For AAT tasks, the target within the AAT sector is used."),
    UpdateInfoBoxNextAltitudeDiff,
    next_waypoint_infobox_panels,
  },

  // e_WP_AltReq
  {
    "Waypoint",
    N_("Next altitude required"),
    N_("WP AltR"),
    N_("Additional altitude required to reach the next turn point. For AAT tasks, the target within the AAT sector is used."),
    UpdateInfoBoxNextAltitudeRequire,
    next_waypoint_infobox_panels,
  },

  // e_WP_Name
  {
    "Waypoint",
    N_("Next waypoint"),
    N_("Next WP"),
    N_("The name of the currently selected turn point. When this InfoBox is active, using the up/down cursor keys selects the next/previous waypoint in the task. (Touch-screen/PC only) Pressing the enter cursor key brings up the waypoint details."),
    IBFHelper<InfoBoxContentNextWaypoint>::Create,
  },

  // e_Fin_AltDiff
  {
    "Task",
    N_("Final altitude difference"),
    N_("Fin AltD"),
    N_("Arrival altitude at the final task turn point relative to the safety arrival height."),
    UpdateInfoBoxFinalAltitudeDiff,
  },

  // e_Fin_AltReq
  {
    "Task",
    N_("Final altitude required"),
    N_("Fin AltR"),
    N_("Additional altitude required to finish the task."),
    UpdateInfoBoxFinalAltitudeRequire,
  },

  // e_SpeedTaskAvg
  {
    "Task",
    N_("Speed task average"),
    N_("V Task Avg"),
    N_("Average cross-country speed while on current task, not compensated for altitude."),
    UpdateInfoBoxTaskSpeed,
  },

  // e_Fin_Distance
  {
    "Task", 
    N_("Final distance"),
    N_("Fin Dist"),
    N_("Distance to finish around remaining turn points."),
    UpdateInfoBoxFinalDistance,
  },

  // e_Fin_GR_TE
  {
    "Task",
    "Final GR (TE) deprecated",
    "---",
    "Deprecated, there is no TE compensation on GR, you should switch to the \"Final GR\" info box.",
    UpdateInfoBoxFinalGR,
  },

  // 20..29 
  // e_H_Terrain
  {
    "GlideComputer",
    N_("Terrain elevation"),
    N_("Terr Elev"),
    N_("This is the elevation of the terrain above mean sea level, obtained from the terrain file at the current GPS location."),
    UpdateInfoBoxTerrainHeight,
  },

  // e_Thermal_Avg
  {
    "Thermal",
    N_("Thermal average"),
    N_("TC Avg"),
    N_("Altitude gained/lost in the current thermal, divided by time spent thermalling."),
    UpdateInfoBoxThermalAvg,
  },

  // e_Thermal_Gain
  {
    "Thermal",
    N_("Thermal gain"),
    N_("TC Gain"),
    N_("The altitude gained/lost in the current thermal."),
    UpdateInfoBoxThermalGain,
  },

  // e_Track_GPS
  {
    "Directions",
    N_("Track"),
    N_("Track"),
    N_("Magnetic track reported by the GPS. (Touch-screen/PC only) If this InfoBox is active in simulation mode, pressing the up and down  arrows adjusts the track."),
    IBFHelper<InfoBoxContentTrack>::Create,
  },

  // e_VerticalSpeed_GPS
  {
    "GlideComputer",
    N_("Vario"),
    N_("Vario"),
    N_("Instantaneous vertical speed, as reported by the GPS, or the intelligent vario total energy vario value if connected to one."),
    UpdateInfoBoxVario,
  },

  // e_WindSpeed_Est
  {
    "Wind",
    N_("Wind speed"),
    N_("Wind"),
    N_("Wind speed estimated by XCSoar or external sensor (if available). "
       "Manual adjustment is possible with the connected InfoBox dialogue. "
       "Pressing the up/down cursor keys to cycle through settings, adjust "
       "the values with left/right cursor keys."),
    UpdateInfoBoxWindSpeed,
    wind_infobox_panels,
  },

  // e_WindBearing_Est
  {
    "Wind",
    N_("Wind bearing"),
    N_("Wind"),
    N_("Wind bearing estimated by XCSoar or external sensor (if available). "
       "Manual adjustment is possible with the connected InfoBox dialogue. "
       "Pressing the up/down cursor keys to cycle through settings, adjust "
       "the values with left/right cursor keys."),
    UpdateInfoBoxWindBearing,
    wind_infobox_panels,
  },

  // e_AA_Time
  {
    "Task",
    N_("AAT time"),
    N_("AAT Time"),
    N_("Assigned Area Task time remaining. Goes red when time remaining has expired."),
    UpdateInfoBoxTaskAATime,
  },

  // e_AA_DistanceMax
  {
    "Task",
    N_("AAT max. distance "),
    N_("AAT Dmax"),
    N_("Assigned Area Task maximum distance possible for remainder of task."),
    UpdateInfoBoxTaskAADistanceMax,
  },

  // e_AA_DistanceMin
  {
    "Task",
    N_("AAT min. distance"),
    N_("AAT Dmin"),
    N_("Assigned Area Task minimum distance possible for remainder of task."),
    UpdateInfoBoxTaskAADistanceMin,
  },

  // 30..39 
  // e_AA_SpeedMax
  {
    "Task",
    N_("AAT speed max. distance"),
    N_("AAT Vmax"),
    N_("Assigned Area Task average speed achievable if flying maximum possible distance remaining in minimum AAT time."),
    UpdateInfoBoxTaskAASpeedMax,
  },

  // e_AA_SpeedMin
  {
    "Task",
    N_("AAT speed min. distance"),
    N_("AAT Vmin"),
    N_("Assigned Area Task average speed achievable if flying minimum possible distance remaining in minimum AAT time."),
    UpdateInfoBoxTaskAASpeedMin,
  },

  // e_AirSpeed_Ext
  {
    "GlideComputer",
    N_("Airspeed IAS"),
    N_("V IAS"),
    N_("Indicated Airspeed reported by a supported external intelligent vario."),
    UpdateInfoBoxSpeedIndicated,
  },

  // e_H_Baro
  {
    "Altitude",
    N_("Barometric altitude"),
    N_("Alt Baro"),
    N_("This is the barometric altitude obtained from a device equipped with a pressure sensor."),
    UpdateInfoBoxAltitudeBaro,
    altitude_infobox_panels,
  },

  // e_WP_Speed_MC
  {
    "GlideComputer",
    N_("Speed MacCready"),
    N_("V MC"),
    N_("The MacCready speed-to-fly for optimal flight to the next waypoint. In cruise flight mode, this speed-to-fly is calculated for maintaining altitude. In final glide mode, this speed-to-fly is calculated for descent."),
    UpdateInfoBoxSpeedMacCready,
  },

  // e_Climb_Perc
  {
    "Thermal",
    N_("Percentage climb"),
    N_("% Climb"),
    N_("Percentage of time spent in climb mode. These statistics are reset upon starting the task."),
    UpdateInfoBoxThermalRatio,
  },

  // e_TimeSinceTakeoff
  {
    "Time", 
    N_("Flight duration"),
    N_("Flt Duration"),
    N_("Time elapsed since takeoff was detected."),
    UpdateInfoBoxTimeFlight,
  },

  // e_Load_G
  {
    "System", 
    N_("G load"),
    N_("G"),
    N_("Magnitude of G loading reported by a supported external intelligent vario. This value is negative for pitch-down manoeuvres."),
    UpdateInfoBoxGLoad,
  },

  // e_WP_GR
  {
    "GlideComputer", 
    N_("Next GR"),
    N_("WP GR"),
    N_("The required glide ratio over ground to reach the next waypoint, given by the distance to next waypoint divided by the height required to arrive at the safety arrival height."),
    UpdateInfoBoxNextGR,
    next_waypoint_infobox_panels,
  },

  // e_TimeLocal
  {
    "Time", 
    N_("Time local"),
    N_("Time loc"),
    N_("GPS time expressed in local time zone."),
    UpdateInfoBoxTimeLocal,
  },

  // 40..49 
  // e_TimeUTC
  {
    "Time", 
    N_("Time UTC"),
    N_("Time UTC"),
    N_("GPS time expressed in UTC."),
    UpdateInfoBoxTimeUTC,
  },

  // e_Fin_Time
  {
    "Time", 
    N_("Task time to go"),
    N_("Fin ETE"),
    N_("Estimated time required to complete task, assuming performance of ideal MacCready cruise/climb cycle."),
    UpdateInfoBoxFinalETE,
  },

  // e_WP_Time
  {
    "Waypoint", 
    N_("Next time to go"),
    N_("WP ETE"),
    N_("Estimated time required to reach next waypoint, assuming performance of ideal MacCready cruise/climb cycle."),
    UpdateInfoBoxNextETE,
    next_waypoint_infobox_panels,
  },

  // e_Act_Speed
  {
    "GlideComputer", 
    N_("Speed dolphin"),
    N_("Vopt"),
    N_("The instantaneous MacCready speed-to-fly, making use of netto vario calculations to determine dolphin cruise speed in the glider's current bearing. In cruise flight mode, this speed-to-fly is calculated for maintaining altitude. In final glide mode, this speed-to-fly is calculated for descent. In climb mode, this switches to the speed for minimum sink at the current load factor (if an accelerometer is connected). When Block mode speed to fly is selected, this InfoBox displays the MacCready speed."),
    UpdateInfoBoxSpeedDolphin,
  },

  // e_VerticalSpeed_Netto
  {
    "GlideComputer", 
    N_("Netto vario"),
    N_("Netto"),
    N_("Instantaneous vertical speed of air-mass, equal to vario value less the glider's estimated sink rate. Best used if airspeed, accelerometers and vario are connected, otherwise calculations are based on GPS measurements and wind estimates."),
    UpdateInfoBoxVarioNetto,
  },

  // e_Fin_TimeLocal
  {
    "Task", 
    N_("Task arrival time"),
    N_("Fin ETA"),
    N_("Estimated arrival local time at task completion, assuming performance of ideal MacCready cruise/climb cycle."),
    UpdateInfoBoxFinalETA,
  },

  // e_WP_TimeLocal
  {
    "Waypoint", 
    N_("Next arrival time"),
    N_("WP ETA"),
    N_("Estimated arrival local time at next waypoint, assuming performance of ideal MacCready cruise/climb cycle."),
    UpdateInfoBoxNextETA,
    next_waypoint_infobox_panels,
  },

  // e_WP_BearingDiff
  {
    "Directions", 
    N_("Bearing difference"),
    N_("Brng D"),
    N_("The difference between the glider's track bearing, to the bearing of the next waypoint, or for AAT tasks, to the bearing to the target within the AAT sector. GPS navigation is based on the track bearing across the ground, and this track bearing may differ from the glider's heading when there is wind present. Chevrons point to the direction the glider needs to alter course to correct the bearing difference, that is, so that the glider's course made good is pointing directly at the next waypoint. This bearing takes into account the curvature of the Earth."),
    UpdateInfoBoxBearingDiff,
  },

  // e_Temperature
  {
    "System", 
    N_("Outside air temperature"),
    N_("OAT"),
    N_("Outside air temperature measured by a probe if supported by a connected intelligent variometer."),
    UpdateInfoBoxTemperature,
  },

  // e_HumidityRel
  {
    "System", 
    N_("Relative humidity"),
    N_("Rel Hum"),
    N_("Relative humidity of the air in percent as measured by a probe if supported by a connected intelligent variometer."),
    UpdateInfoBoxHumidity,
  },

  // 50..59 
  // e_Home_Temperature
  {
    "System", 
    N_("Forecast temperature"),
    N_("Max Temp"),
    N_("Forecast temperature of the ground at the home airfield, used in estimating convection height and cloud base in conjunction with outside air temperature and relative humidity probe. (Touch-screen/PC only) Pressing the up/down cursor keys adjusts this forecast temperature."),
    IBFHelper<InfoBoxContentTemperatureForecast>::Create,
  },

  // e_Fin_AA_Distance
  {
    "Task", 
    N_("AAT distance around target"),
    N_("AAT Dtgt"),
    N_("Assigned Area Task distance around target points for remainder of task."),
    UpdateInfoBoxTaskAADistance,
  },

  // e_AA_SpeedAvg
  {
    "Task", 
    N_("AAT speed around target"),
    N_("AAT Vtgt"),
    N_("Assigned Area Task average speed achievable around target points remaining in minimum AAT time."),
    UpdateInfoBoxTaskAASpeed,
  },

  // e_LD
  {
    "GlideComputer", 
    N_("L/D vario"),
    N_("L/D Vario"),
    N_("Instantaneous lift/drag ratio, given by the indicated airspeed divided by the total energy vertical speed, when connected to an intelligent variometer. Negative values indicate climbing cruise. If the total energy vario speed is close to zero, the displayed value is '---'."),
    UpdateInfoBoxLDVario,
  },

  // e_Speed
  {
    "GlideComputer", 
    N_("Airspeed TAS"),
    N_("V TAS"),
    N_("True Airspeed reported by a supported external intelligent vario."),
    UpdateInfoBoxSpeed,
  },

  // e_Team_Code
  {
    "Team", 
    N_("Team code"),
    N_("Team Code"),
    N_("The current Team code for this aircraft. Use this to report to other team members. The last team aircraft code entered is displayed underneath."),
    IBFHelper<InfoBoxContentTeamCode>::Create,
  },

  // e_Team_Bearing
  {
    "Team", 
    N_("Team bearing"),
    N_("Team Brng"),
    N_("The bearing to the team aircraft location at the last team code report."),
    UpdateInfoBoxTeamBearing,
  },

  // e_Team_BearingDiff
  {
    "Team", 
    N_("Team bearing difference"),
    N_("Team BrngD"),
    N_("The relative bearing to the team aircraft location at the last reported team code."),
    UpdateInfoBoxTeamBearingDiff,
  },

  // e_Team_Range
  {
    "Team", 
    N_("Team range"),
    N_("Team Dist"),
    N_("The range to the team aircraft location at the last reported team code."),
    UpdateInfoBoxTeamDistance,
  },

  // e_CC_SpeedInst
  {
    "Task", 
    N_("Speed task instantaneous"),
    N_("V Task Inst"),
    N_("Instantaneous cross-country speed while on current task, compensated for altitude. Equivalent to instantaneous Pirker cross-country speed."),
    UpdateInfoBoxTaskSpeedInstant,
  },

  // 60..69 
  // e_Home_Distance
  {
    "Home", 
    N_("Distance home"),
    N_("Home Dist"),
    N_("Distance to home waypoint (if defined)."),
    UpdateInfoBoxHomeDistance,
  },

  // e_CC_Speed
  {
    "Task", 
    N_("Speed task achieved"),
    N_("V Task Ach"),
    N_("Achieved cross-country speed while on current task, compensated for altitude.  Equivalent to Pirker cross-country speed remaining."),
    UpdateInfoBoxTaskSpeedAchieved,
  },

  // e_AA_TimeDiff
  {
    "Task", 
    N_("AAT delta time"),
    N_("AAT dT"),
    N_("Difference between estimated task time and AAT minimum time. Coloured red if negative (expected arrival too early), or blue if in sector and can turn now with estimated arrival time greater than AAT time plus 5 minutes."),
    UpdateInfoBoxTaskAATimeDelta,
  },

  // e_Climb_Avg
  {
    "Thermal", 
    N_("Thermal average over all"),
    N_("T Avg"),
    N_("Time-average climb rate in all thermals."),
    UpdateInfoBoxThermalAllAvg,
  },

  // e_RH_Trend
  {
    "Task", 
    N_("Task req. total height trend"),
    N_("RH Trend"),
    N_("Trend (or neg. of the variation) of the total required height to complete the task."),
    UpdateInfoBoxVarioDistance,
  },

  // e_Battery
  {
    "System", 
    N_("Battery percent"),
    N_("Battery"),
    N_("Displays percentage of device battery remaining (where applicable) and status/voltage of external power supply."),
    UpdateInfoBoxBattery,
  },

  // e_Fin_GR
  {
    "Task", 
    N_("Final GR"),
    N_("Fin GR"),
    N_("The required glide ratio over ground to finish the task, given by the distance to go divided by the height required to arrive at the safety arrival height."),
    UpdateInfoBoxFinalGR,
  },

  // e_Alternate_1_Name
  {
    "GlideComputer", 
    N_("Alternate 1"),
    N_("Altn 1"),
    N_("Displays name and bearing to the best alternate landing location."),
    IBFHelperInt<InfoBoxContentAlternateName, 0>::Create,
  },

  // e_Alternate_2_Name
  {
    "GlideComputer", 
    N_("Alternate 2"),
    N_("Altn 2"),
    N_("Displays name and bearing to the second alternate landing location."),
    IBFHelperInt<InfoBoxContentAlternateName, 1>::Create,
  },

  // e_Alternate_1_GR
  {
    "GlideComputer", 
    N_("Alternate 1 GR"),
    N_("Altn1 GR"),
    N_("Geometric gradient to the arrival height above the best alternate. This is not adjusted for total energy."),
    IBFHelperInt<InfoBoxContentAlternateGR, 0>::Create,
  },

  // 70..79 
  // e_H_QFE
  {
    "Altitude", 
    N_("Height above take-off"),
    N_("H T/O"),
    N_("Height based on an automatic take-off reference elevation (like a QFE reference)."),
    UpdateInfoBoxAltitudeQFE,
    altitude_infobox_panels,
  },

  // e_GR_Avg
  {
    "Task", 
    N_("GR average"),
    N_("GR Avg"),
    N_("The distance made in the configured period of time , divided by the altitude lost since then. Negative values are shown as ^^^ and indicate climbing cruise (height gain). Over 200 of GR the value is shown as +++ . You can configure the period of averaging in the system setup. Suggested values are 60, 90 or 120. Lower values will be closed to GR Inst, and higher values will be closed to GR Cruise. Notice that the distance is NOT the straight line between your old and current position, it's exactly the distance you have made even in a zigzag glide. This value is not calculated while circling."),
    UpdateInfoBoxGRAvg,
  },

  // e_Experimental
  {
    "Task", 
    N_("Experimental 1"),
    N_("Exp1"),
    NULL,
    UpdateInfoBoxExperimental1,
  },

  // e_Contest_Distance
  {
    "Task", 
    N_("Contest distance"),
    N_("Cont Dist"),
    N_("Instantaneous evaluation of the flown distance according to the configured Contest rule set."),
    IBFHelper<InfoBoxContentContest>::Create,
  },

  // e_Experimental2
  {
    "Task", 
    N_("Experimental 2"),
    N_("Exp2"),
    NULL,
    UpdateInfoBoxExperimental2,
  },

  // e_CPU_Load
  {
    "Task", 
    N_("CPU load"),
    N_("CPU"),
    N_("CPU load consumed by XCSoar averaged over 5 seconds."),
    UpdateInfoBoxCPULoad,
  },

  // e_WP_H
  {
    "Task", 
    N_("Next altitude arrival"),
    N_("WP AltA"),
    N_("Absolute arrival altitude at the next waypoint in final glide.  For AAT tasks, the target within the AAT sector is used."),
    UpdateInfoBoxNextAltitudeArrival,
    next_waypoint_infobox_panels,
  },

  // e_Free_RAM
  {
    "Task", 
    N_("Free RAM"),
    N_("Free RAM"),
    N_("Free RAM as reported by OS."),
    UpdateInfoBoxFreeRAM,
  },

  // e_FlightLevel
  {
    "Task", 
    N_("Flight level"),
    N_("FL"),
    N_("Pressure Altitude given as Flight Level. If barometric altitude is not available, FL is calculated from GPS altitude, given that the correct QNH is set. In case the FL is calculated from the GPS altitude, the FL label is coloured red."),
    UpdateInfoBoxAltitudeFlightLevel,
    altitude_infobox_panels,
  },

  // e_Barogram
  {
    "Task", 
    N_("Barogram"),
    N_("Barogram"),
    N_("Trace of altitude during flight"),
    IBFHelper<InfoBoxContentBarogram>::Create,
  },

  // 80..89 
  // e_Vario_spark
  {
    "Thermal", 
    N_("Vario trace"),
    N_("Vario Trace"),
    N_("Trace of vertical speed, as reported by the GPS, or the intelligent vario total energy vario value if connected to one."),
    IBFHelper<InfoBoxContentVarioSpark>::Create,
  },

  // e_NettoVario_spark
  {
    "Thermal", 
    N_("Netto vario trace"),
    N_("Netto Trace"),
    N_("Trace of vertical speed of air-mass, equal to vario value less the glider's estimated sink rate."),
    IBFHelper<InfoBoxContentNettoVarioSpark>::Create,
  },

  // e_CirclingAverage_spark
  {
    "Thermal", 
    N_("Thermal climb trace"),
    N_("TC Trace"),
    N_("Trace of average climb rate each turn in circling, based of the reported GPS altitude, or vario if available."),
    IBFHelper<InfoBoxContentCirclingAverageSpark>::Create,
  },

  // e_ThermalBand
  {
    "Thermal", 
    N_("Climb band"),
    N_("Climb Band"),
    N_("Graph of average circling climb rate (horizontal axis) as a function of altitude (vertical axis)."),
    IBFHelper<InfoBoxContentThermalBand>::Create,
  },

  // e_TaskProgress
  {
    "Task", 
    N_("Task progress"),
    N_("Progress"),
    N_("Clock-like display of distance remaining along task, showing achieved task points."),
    IBFHelper<InfoBoxContentTaskProgress>::Create,
  },

  // e_TaskMaxHeightTime
  {
    "Task", 
    N_("Time under max. start height"),
    N_("Start Height"),
    N_("The contiguous period the ship has been below the task start max. height."),
    UpdateInfoBoxTaskTimeUnderMaxHeight,
  },

  // e_Fin_ETE_VMG
  {
    "Task", 
    N_("Task time to go (ground speed)"),
    N_("Fin ETE VMG"),
    N_("Estimated time required to complete task, assuming current ground speed is maintained."),
    UpdateInfoBoxFinalETEVMG,
  },

  // e_WP_ETE_VMG
  {
    "Task", 
    N_("Next time to go (ground speed)"),
    N_("WP ETE VMG"),
    N_("Estimated time required to reach next waypoint, assuming current ground speed is maintained."),
    UpdateInfoBoxNextETEVMG,
    next_waypoint_infobox_panels,
  },

  // e_Horizon
  {
    "System", 
    N_("Attitude indicator"),
    N_("Horizon"),
    N_("Attitude indicator (artificial horizon) display calculated from flight path, supplemented with acceleration and variometer data if available."),
    IBFHelper<InfoBoxContentHorizon>::Create,
  },

  // e_NearestAirspaceHorizontal
  {
    "GlideComputer", 
    N_("Nearest airspace horizontal"),
    N_("Near AS H"),
    N_("The horizontal distance to the nearest airspace."),
    UpdateInfoBoxNearestAirspaceHorizontal,
  },

  // 90..99 
  // e_NearestAirspaceVertical
  {
    "Airspace", 
    N_("Nearest airspace vertical"),
    N_("Near AS V"),
    N_("The vertical distance to the nearest airspace.  A positive value means the airspace is above you, and negative means the airspace is below you."),
    UpdateInfoBoxNearestAirspaceVertical,
  },

  // e_WP_MC0AltDiff
  {
    "GlideComputer", 
    N_("Next MC0 altitude difference"),
    N_("WP MC0 AltD"),
    N_("Arrival altitude at the next waypoint with MC 0 setting relative to the safety arrival height.  For AAT tasks, the target within the AAT sector is used."),
    UpdateInfoBoxNextMC0AltitudeDiff,
    next_waypoint_infobox_panels,
  },

  // e_HeadWind
  {
    "Wind", 
    N_("Wind, head component"),
    N_("Head Wind"),
    N_("The current head wind component. Head wind is calculated from TAS and GPS ground speed if airspeed is available from external device. Otherwise the estimated wind is used for the calculation."),
    UpdateInfoBoxHeadWind,
    wind_infobox_panels,
  },

  // e_TerrainCollision
  {
    "GlideComputer", 
    N_("Terrain collision"),
    N_("Terr Coll"),
    N_("The distance to the next terrain collision along the current task leg. At this location, the altitude will be below the configured terrain clearance altitude."),
    UpdateInfoBoxTerrainCollision,
  },

   // e_NavAltitude
  {
    "Altitude", 
    N_("Altitude (Auto)"),
    N_("Alt Auto"),
    N_("This is the barometric altitude obtained from a device equipped with a pressure sensor or the GPS altitude if the barometric altitude is not available."),
    UpdateInfoBoxAltitudeNav,
    altitude_infobox_panels,
  },

  // e_NextLegEqThermal
  {
    "Thermal", 
    N_("Thermal next leg equivalent"),
    N_("T Next Leg"),
    N_("The thermal rate of climb on next leg which is equivalent to a thermal equal to the MacCready setting on current leg."),
    UpdateInfoBoxNextLegEqThermal,
  },

  // e_HeadWindSimplified
  {
    "Wind", 
    N_("Wind, head component (simplified)"),
    N_("Head Wind *"),
    N_("The current head wind component. The simplified head wind is calculated by subtracting GPS ground speed from the TAS if airspeed is available from external device."),
    UpdateInfoBoxHeadWindSimplified,
    wind_infobox_panels,
  },

  // e_CruiseEfficiency
  {
    "GlideComputer", 
    N_("Task cruise efficiency"),
    N_("Cruise Eff"),
    N_("Efficiency of cruise.  100 indicates perfect MacCready performance. "
       "This value estimates your cruise efficiency according to the current "
       "flight history with the set MC value.  Calculation begins after task is started."),
    UpdateInfoBoxCruiseEfficiency,
  },

  // e_WindArrow
  {
    "Wind", 
    N_("Wind arrow"),
    N_("Wind"),
    N_("Wind speed estimated by XCSoar or external sensor (if available). "
       "Manual adjustment is possible with the connected InfoBox dialogue. "
       "Pressing the up/down cursor keys to cycle through settings, adjust "
       "the values with left/right cursor keys."),
    IBFHelper<InfoBoxContentWindArrow>::Create,
  },

  // e_ThermalAssistant
  {
    "Thermal", 
    N_("Thermal assistant"),
    N_("Thermal"),
    N_("A circular thermal assistant that shows the lift distribution over each part of the circle."),
    IBFHelper<InfoBoxContentThermalAssistant>::Create,
  },

  // 100..109
  // e_StartOpenLine
  {
    "Task", 
    N_("Start open/close countdown"),
    N_("Start open"),
    N_("Shows the time left until the start point opens or closes."),
    UpdateInfoBoxStartOpen,
  },

  // e_StartOpenArrivalLine
  {
    "Task", 
    N_("Start open/close countdown at reaching"),
    N_("Start reach"),
    N_("Shows the time left until the start point opens or closes, compared to the calculated time to reach it."),
    UpdateInfoBoxStartOpenArrival,
  },

  // e_NextRadial,
  {
    "Directions", 
    N_("Next radial"),
    N_("Radial"),
    N_("True bearing from the next waypoint to your position."),
    UpdateInfoBoxRadial,
    next_waypoint_infobox_panels,
  },

  // e_ATC_Radial
  {
    "Directions", 
    N_("ATC radial"),
    N_("ATC radial"),
    N_("Bearing from the selected reference location to your position. The distance is displayed in nautical miles for communication with ATC. If declination is entered, magnetic bearing is given to match VOR radials."),
    UpdateInfoBoxATCRadial,
    atc_infobox_panels,
  },

  // e_TaskSpeedHour
  {
    "Task", 
    N_("Speed task last hour"),
    N_("V Task H"),
    N_("Average cross-country speed while on current task over the last hour, not compensated for altitude."),
    UpdateInfoBoxTaskSpeedHour,
  },

  // e_WP_NominalDistance
  {
    "Waypoint", 
    N_("Next distance (nominal)"),
    N_("WP Dist-N"),
    N_("The distance to the currently selected waypoint. For AAT tasks, this is the distance to the origin of the AAT sector."),
    UpdateInfoBoxNextDistanceNominal,
    next_waypoint_infobox_panels,
  },

  // e_CircleDiameter
  {
    "Thermal", 
    N_("Circle diameter"),
    N_("Circle D"),
    N_("Circle diameter. Displays estimated circle diameter and full circle flight time. Useful for evaluating best thermalling mode with a glider at different wing loading."),
    UpdateInfoBoxCircleDiameter,
  },

  // e_TakeOffDistance
  {
    "Home", 
    N_("Distance takeoff"),
    N_("Takeoff Dist"),
    N_("Distance to where take-off was detected."),
    UpdateInfoBoxTakeoffDistance,
  },

  // e_ContestSpeed
  {
    "Task", 
    N_("Contest speed"),
    N_("Cont Speed"),
    N_("Instantaneous evaluation of the flown speed according to the configured Contest rule set."),
    IBFHelper<InfoBoxContentContestSpeed>::Create,
  },

  // e_Final_MC0_Altitude
  {
    "Task", 
    N_("Final MC0 altitude difference"),
    N_("Fin MC0 AltD"),
    N_("Arrival altitude at the final waypoint with MC 0 setting relative to the safety arrival height."),
    UpdateInfoBoxFinalMC0AltitudeDiff,
  },

  // 110..119
  // e_NextWPArrow
  {
    "Waypoint", 
    N_("Next arrow"),
    N_("Next arrow"),
    N_("Arrow pointing to the currently selected waypoint. The name of the "
       "waypoint and the distance are also shown. "
       "The position used is the optimized position of the active waypoint "
       "in the current task, the center of a selected goto waypoint "
       "or the target within the AAT sector for AAT tasks."),
    IBFHelper<InfoBoxContentNextArrow>::Create,
  },

  // e_WP_Estimation
  {
    "Waypoint", 
    N_("Next waypoint arrival time (ground speed)"),
    N_("WP ETA VMG"),
    N_("Estimated arrival time at next waypoint, assuming current ground speed is maintained."),
    UpdateInfoBoxNextETAVMG,
    next_waypoint_infobox_panels,
  },

  // e_NonCircling_Climb_Perc
  {
    "Thermal", 
    N_("Percentage non-circling climb"),
    N_("% Str Climb"),
    N_("Percentage of time spent climbing without circling. These statistics are reset upon starting the task."),
    UpdateInfoBoxNonCirclingClimbRatio,
  },

  // e_Climb_Perc_Chart
  {
    "Thermal", 
    N_("Percentage climb chart"),
    N_("Climb %"),
    N_("Pie chart of time circling and climbing, circling and descending, and climbing non-circling."),
    IBFHelper<InfoBoxContentClimbPercent>::Create,
  },

  // Number Satellites
  {
    "System", 
    N_("Number of used satellites"),
    N_("Satellites"),
    N_("The number of actually used (seen) satellites by GPS module. If this information is unavailable, the displayed value is '---'."),
    UpdateInfoBoxNbrSat,
  },

  // Radio
  {
    "System", 
    N_("Active Radio Frequency"),
    N_("Act Freq"),
    N_("The currently active Radio Frequency"),
    IBFHelper<InfoBoxContentActiveRadioFrequency>::Create,
  },

  // e_StandbyRadio
  {
    "System", 
    N_("Standby Radio Frequency"),
    N_("Stby Freq"),
    N_("The currently standby Radio Frequency"),
    IBFHelper<InfoBoxContentStandbyRadioFrequency>::Create,
  },

  // e_Thermal_Time
  {
    "Thermal", 
    N_("Thermal time"),
    N_("TC Time"),
    N_("The time spend in the current thermal."),
    UpdateInfoBoxThermalTime,
  },

  // e_Alternate_2_GR
  {
    "GlideComputer", 
    N_("Alternate 2 GR"),
    N_("Altn2 GR"),
    N_("Geometric gradient to the arrival height above the second alternate. This is not adjusted for total energy."),
    IBFHelperInt<InfoBoxContentAlternateGR, 1>::Create,
  },

  // e_HeartRate
  {
    "System", 
    N_("Heart Rate"),
    N_("Heart"),
    N_("Heart rate in beats per minute."),
    UpdateInfoBoxHeartRate,
  },

  // 120.. 
  // e_NUM_TYPES // Last item 
  // Transponder code
  {
    "System", 
    N_("Transponder Code"),
    N_("XPDR Code"),
    N_("The currently set Transponder code"),
    IBFHelper<InfoBoxContentTransponderCode>::Create,
  },

  // e_EngineTempCHT
  {
    "System", 
    N_("Engine CHT"),
    N_("CHT"),
    N_("Engine Cylinder Head Temperature"),
    UpdateInfoBoxContentCHT,
  },

  // e_EngineTempEGT
  {
    "System", 
    N_("Engine EGT"),
    N_("EGT"),
    N_("Engine Exhaust Gas Temperature"),
    UpdateInfoBoxContentEGT,
  },

  // e_EngineRPM
  {
    "System", 
    N_("Engine Revolutions Per Minute"),
    N_("RPM"),
    N_("Engine Revolutions Per Minute."),
    UpdateInfoBoxContentRPM,
  },

  // e_AAT_dT_or_ETA
  {
    "Task", 
    N_("AAT dT and task ETA"),
    N_("AATdeltaOrETA"),
    N_("Shows AAT delta time and estimated time of arrival in case of AAT task, and estimated time of arrival in case of racing task"),
    UpdateInfoTaskETAorAATdT,
  },

  // e_SpeedTaskEst
  {
    "Task", 
    N_("Speed task estimated"),
    N_("V Task Est"),
    N_("Estimated average cross-country speed for current task as of task completion, assuming performance of ideal MacCready cruise/climb cycle."),
    UpdateInfoBoxTaskSpeedEst,
  },

  // e_Home_AltDiff
  {
    "Altitude", 
    N_("Home altitude difference"),
    N_("Home AltD"),
    N_("Arrival altitude at the home waypoint relative to the safety arrival height."),
    UpdateInfoBoxHomeAltitudeDiff,
  },

};

// static_assert(ARRAY_SIZE(meta_data) == NUM_TYPES, "Wrong InfoBox factory size");

static constexpr MetaData meta_data_2nd[] = {
    // 500..
    // e_DriftAngle - TestBox for Track-Heading
    {
        "Wind", 
        N_("Wind - Drift Angle"),
        N_("Wind Drift"),
        N_("Wind drift angle (Track - True Heading) - for a simple check, "
           "how the sensor works."),
        IBFHelper<InfoBoxDrift>::Create,
    },

    // e_InstantaneousWindSpeed
    {
        "Wind", 
        N_("Wind - RealTime Speed"),
        N_("RealTime Wind"),
        N_("Speed of instantaneous wind, estimated from an external sensor."),
        UpdateInfoBoxInstWindSpeed,
        wind_infobox_panels,
    },

    // e_InstantaneousWindBearing
    {
        "Wind", 
        N_("Wind - RealTime Bearing"),
        N_("RealTime Wind"),
        N_("Bearing of instantaneous wind, estimated from an external sensor."),
        UpdateInfoBoxInstWindBearing,
        wind_infobox_panels,
    },

    // e_InternalWind
    {
        "Wind", 
        N_("Wind - Internal Wind"),
        N_("Internal Wind"),
        N_("Bearing of instantaneous wind, estimated from an external sensor."),
        UpdateInfoBoxInstWindBearing,
        wind_infobox_panels,
    },

    // e_InternalZigZagWind
    {
        "Wind", 
        N_("Wind - Internal ZigZag Wind"),
        N_("ZigZag Wind"),
        N_("Bearing of instantaneous wind, estimated from an external sensor."),
        UpdateInfoBoxInstWindBearing,
        wind_infobox_panels,
    },

    // e_PageNo
    {
        "System", 
        N_("Page No"),
        N_("Page No"),
        N_("Index of displayed page."),
        UpdateInfoBoxPageIndex,
    },

    // e_STFSwitch
    {
        "System", 
        N_("STF Switch"),
        N_("STF Switch"),
        N_("State of the STV/Vario switch."),
        UpdateInfoBoxSTFSwitch,
    },

};

// static_assert(ARRAY_SIZE(meta_data_2nd) == NUM_TYPES_2nd - e_NUM_AREA_2nd, "Wrong InfoBox factory size");

const char *
InfoBoxFactory::GetName(Type type) noexcept
{
  assert(TypeIsValid(type));
  return (type < NUM_TYPES) ? meta_data[type].name
                            : meta_data_2nd[type - e_NUM_AREA_2nd].name;
}

const char *
InfoBoxFactory::GetCaption(Type type) noexcept
{
  assert(TypeIsValid(type));
  return (type < NUM_TYPES) ? meta_data[type].caption : 
    meta_data_2nd[type - e_NUM_AREA_2nd].caption;

}

/**
 * Returns the long description (help text) of the info box type.
 */
const char *
InfoBoxFactory::GetDescription(Type type) noexcept
{
  assert(TypeIsValid(type));
  return (type < NUM_TYPES) ? meta_data[type].description
                            : meta_data_2nd[type - e_NUM_AREA_2nd].description;
}

std::unique_ptr<InfoBoxContent>
InfoBoxFactory::Create(Type type) noexcept
{
  assert(TypeIsValid(type));

  const auto &m = type < NUM_TYPES ? meta_data[type]
                      : meta_data_2nd[type - e_NUM_AREA_2nd];

  assert(m.create != nullptr ||
         m.update != nullptr);

  if (m.create != nullptr)
    return std::unique_ptr<InfoBoxContent>(m.create());
  else
    return std::make_unique<InfoBoxContentCallback>(m.update, m.panels);
}

bool 
InfoBoxFactory::TypeIsValid(Type t)
{
  return (t < e_NUM_TYPES) || ((t >= e_NUM_AREA_2nd) && (t < e_NUM_TYPES_2nd));
}
