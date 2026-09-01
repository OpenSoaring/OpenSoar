// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "InfoBoxes/Content/Extension.hpp"
#include "InfoBoxes/Content/Base.hpp"
#include "InfoBoxes/Content/OpenSoar.hpp"
#include "Language/Language.hpp"
#include "util/Macros.hpp"

#include <cassert>

using namespace InfoBoxFactory;

/**
 * An InfoBox whose content is a plain update function (the OpenSoar
 * block has no panels).
 */
class InfoBoxContentUpdate final : public InfoBoxContent {
  void (*const update)(InfoBoxData &data) noexcept;

public:
  explicit InfoBoxContentUpdate(void (*_update)(InfoBoxData &data) noexcept) noexcept
    :update(_update) {}

  void Update(InfoBoxData &data) noexcept override {
    update(data);
  }
};

/**
 * One entry of the OpenSoar block.  A placeholder (no update
 * function) keeps a number occupied that the old OpenSoar used for a
 * box this version does not carry.
 */
struct ExtensionMetaData {
  Group group;

  const char *name;
  const char *caption;
  const char *description;

  void (*update)(InfoBoxData &data) noexcept;

  /**
   * The upstream type XCSoar has adopted this box under, or NUM_TYPES
   * while it is still OpenSoar's alone.  Once set, Resolve() maps the
   * OpenSoar number to it and the picker hides this entry.
   */
  Type upstream;

  constexpr bool IsPlaceholder() const noexcept {
    return update == nullptr;
  }

  constexpr bool IsSuperseded() const noexcept {
    return upstream != NUM_TYPES;
  }
};

/* WARNING: the order is the order of the enum (Type.hpp) and of the
   old OpenSoar profiles.  Never insert, delete or rearrange - a box
   that goes away becomes a placeholder. */
static constexpr ExtensionMetaData opensoar_meta_data[] = {
  // e_DriftAngle (500): test box of the old OpenSoar, not carried
  { Group::OTHER, nullptr, nullptr, nullptr, nullptr, NUM_TYPES },

  // e_InstantaneousWindSpeed
  { Group::WIND,
    N_("Wind - live speed"),
    N_("Live Wind"),
    N_("Speed of the instantaneous wind reported by an external sensor "
       "(Anemoi, ...), unfiltered; the bearing in the comment."),
    UpdateInfoBoxInstantaneousWindSpeed, NUM_TYPES },

  // e_InstantaneousWindBearing
  { Group::WIND,
    N_("Wind - live bearing"),
    N_("Live Wind"),
    N_("Bearing of the instantaneous wind reported by an external "
       "sensor (Anemoi, ...), unfiltered; the speed in the comment."),
    UpdateInfoBoxInstantaneousWindBearing, NUM_TYPES },

  // e_InternalWind
  { Group::WIND,
    N_("Wind - internal estimate"),
    N_("Int Wind"),
    N_("The wind XCSoar estimates itself from circling and from the "
       "EKF, whatever the effective wind is set to (external sensor, "
       "manual)."),
    UpdateInfoBoxInternalWind, NUM_TYPES },

  // e_InternalZigZagWind
  { Group::WIND,
    N_("Wind - zig-zag estimate"),
    N_("ZigZag Wind"),
    N_("The wind of the zig-zag (EKF) estimator, shown while it is the "
       "source of the effective wind."),
    UpdateInfoBoxInternalZigZagWind, NUM_TYPES },

  // e_PageNo
  { Group::SYSTEM,
    N_("Page number"),
    N_("Page"),
    N_("Number of the page on display and the number of pages, "
       "with the kind of the page in the comment."),
    UpdateInfoBoxPageNo, NUM_TYPES },

  // e_STFSwitch
  { Group::SETTING,
    N_("STF switch"),
    N_("STF"),
    N_("State of the speed-to-fly / vario switch as reported by the "
       "connected vario."),
    UpdateInfoBoxSTFSwitch, NUM_TYPES },

  // e_BugsSetting
  { Group::SETTING,
    N_("Bugs setting"),
    N_("Bugs"),
    N_("The bugs setting: 100 % is a clean wing."),
    UpdateInfoBoxBugsSetting, NUM_TYPES },

  // e_TrueHeading
  { Group::SPEED,
    N_("True heading"),
    N_("Heading"),
    N_("True heading from the attitude sensor of a connected device."),
    UpdateInfoBoxTrueHeading, NUM_TYPES },

  // e_WaterBallast
  { Group::SETTING,
    N_("Water ballast"),
    N_("Ballast"),
    N_("Water ballast in litres, with the share of the maximum in the "
       "comment."),
    UpdateInfoBoxWaterBallast, NUM_TYPES },

  // e_Mouse, e_Coordinates, e_MouseDistance: developer boxes of the
  // old OpenSoar, not carried
  { Group::OTHER, nullptr, nullptr, nullptr, nullptr, NUM_TYPES },
  { Group::OTHER, nullptr, nullptr, nullptr, nullptr, NUM_TYPES },
  { Group::OTHER, nullptr, nullptr, nullptr, nullptr, NUM_TYPES },
};

static_assert(ARRAY_SIZE(opensoar_meta_data) == NUM_OPENSOAR_TYPES,
              "Wrong OpenSoar InfoBox table size");

static constexpr bool
IsOpenSoarType(Type type) noexcept
{
  return type >= OPENSOAR_FIRST && type < OPENSOAR_END;
}

static constexpr const ExtensionMetaData &
GetExtension(Type type) noexcept
{
  assert(IsOpenSoarType(type));
  return opensoar_meta_data[type - OPENSOAR_FIRST];
}

/*
 * groups
 */

const char *
InfoBoxFactory::GetGroupName(Group group) noexcept
{
  switch (group) {
  case Group::ALTITUDE: return N_("Altitude");
  case Group::VARIO: return N_("Vario and thermal");
  case Group::GLIDE: return N_("Glide");
  case Group::SPEED: return N_("Speed and attitude");
  case Group::WIND: return N_("Wind and weather");
  case Group::WAYPOINT: return N_("Waypoint");
  case Group::TASK: return N_("Task");
  case Group::TIME: return N_("Time");
  case Group::AIRSPACE_TEAM: return N_("Airspace and team");
  case Group::SETTING: return N_("Settings");
  case Group::SYSTEM: return N_("System");
  case Group::OTHER: return N_("Other");
  case Group::COUNT: break;
  }

  return N_("Other");
}

/**
 * The group of an upstream InfoBox.  Kept here rather than in the
 * upstream table so that an XCSoar update never conflicts; a new
 * upstream box lands under "Other" until it is sorted in.
 */
static constexpr Group
GetUpstreamGroup(Type type) noexcept
{
  switch (type) {
  case e_HeightGPS:
  case e_HeightAGL:
  case e_H_Terrain:
  case e_H_Baro:
  case e_H_QFE:
  case e_WP_H:
  case e_FlightLevel:
  case e_Barogram:
  case e_AltitudeIGC:
  case e_QNH:
  case NavAltitude:
  case TerrainCollision:
    return Group::ALTITUDE;

  case e_Thermal_30s:
  case e_TL_Avg:
  case e_TL_Gain:
  case e_TL_Time:
  case e_Thermal_Avg:
  case e_Thermal_Gain:
  case e_VerticalSpeed_GPS:
  case e_Climb_Perc:
  case e_VerticalSpeed_Netto:
  case e_Climb_Avg:
  case e_Vario_spark:
  case e_NettoVario_spark:
  case e_CirclingAverage_spark:
  case e_ThermalBand:
  case e_NonCircling_Climb_Perc:
  case e_Climb_Perc_Chart:
  case e_Thermal_Time:
  case NextLegEqThermal:
  case THERMAL_ASSISTANT:
    return Group::VARIO;

  case e_GR_Instantaneous:
  case e_GR_Cruise:
  case e_WP_AltDiff:
  case e_WP_AltReq:
  case e_Fin_AltDiff:
  case e_Fin_AltReq:
  case e_Fin_GR_TE:
  case e_WP_GR:
  case e_LD:
  case e_Fin_GR:
  case e_GR_Avg:
  case e_WP_MC0AltDiff:
  case e_RH_Trend:
  case CruiseEfficiency:
  case FIN_MC0_ALTD:
    return Group::GLIDE;

  case e_Speed_GPS:
  case e_Track_GPS:
  case e_AirSpeed_Ext:
  case e_Load_G:
  case e_Act_Speed:
  case e_Speed:
  case e_Horizon:
  case CIRCLE_DIAMETER:
    return Group::SPEED;

  case e_WindSpeed_Est:
  case e_WindBearing_Est:
  case e_Temperature:
  case e_HumidityRel:
  case e_Home_Temperature:
  case e_HeadWind:
  case HeadWindSimplified:
  case WIND_ARROW:
    return Group::WIND;

  case e_Bearing:
  case e_WP_Distance:
  case e_WP_Name:
  case e_WP_Speed_MC:
  case e_WP_Time:
  case e_WP_TimeLocal:
  case e_WP_BearingDiff:
  case e_Home_Distance:
  case e_Alternate_1_Name:
  case e_Alternate_2_Name:
  case e_Alternate_1_GR:
  case e_WP_ETE_VMG:
  case e_WP_ETA_VMG:
  case e_Alternate_2_GR:
  case e_Home_AltDiff:
  case e_Alternate_1_AltDiff:
  case e_Alternate_2_AltDiff:
  case e_Home:
  case e_ActiveWaypoint:
  case e_PreviousWaypoint:
  case NEXT_RADIAL:
  case ATC_RADIAL:
  case WP_NOMINAL_DIST:
  case NEXT_ARROW:
  case TAKEOFF_DISTANCE:
    return Group::WAYPOINT;

  case e_MacCready:
  case e_SpeedTaskAvg:
  case e_Fin_Distance:
  case e_AA_Time:
  case e_AA_DistanceMax:
  case e_AA_DistanceMin:
  case e_AA_SpeedMax:
  case e_AA_SpeedMin:
  case e_Fin_Time:
  case e_Fin_TimeLocal:
  case e_Fin_AA_Distance:
  case e_AA_SpeedAvg:
  case e_CC_SpeedInst:
  case e_CC_Speed:
  case e_AA_TimeDiff:
  case e_OC_Distance:
  case e_TaskProgress:
  case e_TaskMaxHeightTime:
  case e_Fin_ETE_VMG:
  case e_AAT_dT_or_ETA:
  case e_SpeedTaskEst:
  case e_SpeedTaskLeg:
  case START_OPEN_TIME:
  case START_OPEN_ARRIVAL_TIME:
  case TASK_SPEED_HOUR:
  case CONTEST_SPEED:
    return Group::TASK;

  case e_TimeSinceTakeoff:
  case e_TimeLocal:
  case e_TimeUTC:
    return Group::TIME;

  case e_Team_Code:
  case e_Team_Bearing:
  case e_Team_BearingDiff:
  case e_Team_Range:
  case e_NearestAirspaceHorizontal:
  case e_NearestAirspaceVertical:
    return Group::AIRSPACE_TEAM;

  case e_ActiveRadio:
  case e_StandbyRadio:
  case e_TransponderCode:
    return Group::SETTING;

  case e_Battery:
  case e_CPU_Load:
  case e_Free_RAM:
  case e_NbrSat:
  case e_HeartRate:
  case e_EngineCHT:
  case e_EngineEGT:
  case e_EngineRPM:
    return Group::SYSTEM;

  case e_Experimental1:
  case e_Experimental2:
  case e_NUM_TYPES:
    break;

  /* the OpenSoar block is handled by GetGroup() */
  case e_DriftAngle:
  case e_InstantaneousWindSpeed:
  case e_InstantaneousWindBearing:
  case e_InternalWind:
  case e_InternalZigZagWind:
  case e_PageNo:
  case e_STFSwitch:
  case e_BugsSetting:
  case e_TrueHeading:
  case e_WaterBallast:
  case e_Mouse:
  case e_Coordinates:
  case e_MouseDistance:
  case e_OPENSOAR_END:
    break;
  }

  return Group::OTHER;
}

Group
InfoBoxFactory::GetGroup(Type type) noexcept
{
  if (IsOpenSoarType(type))
    return GetExtension(type).group;

  return GetUpstreamGroup(type);
}

/*
 * validity and hand-over
 */

bool
InfoBoxFactory::IsValid(Type type) noexcept
{
  return type < NUM_TYPES || IsOpenSoarType(type);
}

bool
InfoBoxFactory::IsSelectable(Type type) noexcept
{
  if (type < NUM_TYPES)
    return true;

  if (!IsOpenSoarType(type))
    return false;

  const auto &m = GetExtension(type);
  return !m.IsPlaceholder() && !m.IsSuperseded();
}

Type
InfoBoxFactory::Resolve(Type type) noexcept
{
  if (!IsOpenSoarType(type))
    return type;

  const auto &m = GetExtension(type);
  return m.IsSuperseded() ? m.upstream : type;
}

/*
 * the factory functions for the OpenSoar block
 */

const char *
InfoBoxFactory::Extension::GetName(Type type) noexcept
{
  if (!IsOpenSoarType(type))
    return nullptr;

  const auto &m = GetExtension(type);
  /* a placeholder has a name in the picker's "invalid" sense: none */
  return m.name;
}

const char *
InfoBoxFactory::Extension::GetCaption(Type type) noexcept
{
  if (!IsOpenSoarType(type))
    return nullptr;

  return GetExtension(type).caption;
}

const char *
InfoBoxFactory::Extension::GetDescription(Type type) noexcept
{
  if (!IsOpenSoarType(type))
    return nullptr;

  return GetExtension(type).description;
}

std::unique_ptr<InfoBoxContent>
InfoBoxFactory::Extension::Create(Type type) noexcept
{
  if (!IsOpenSoarType(type))
    return nullptr;

  const auto &m = GetExtension(type);
  if (m.IsPlaceholder())
    return nullptr;

  return std::make_unique<InfoBoxContentUpdate>(m.update);
}
