// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GlueLogbook.hpp"
#include "Logger.hpp"
#include "Blackboard/LiveBlackboard.hpp"
#include "Engine/Waypoint/Waypoints.hpp"
#include "Engine/Waypoint/Waypoint.hpp"
#include "Formatter/GeoPointFormatter.hpp"
#include "NMEA/MoreData.hpp"
#include "NMEA/Derived.hpp"
#include "Computer/Settings.hpp"
#include "UISettings.hpp"
#include "LogFile.hpp"

#include <algorithm>
#include <cmath>

using std::chrono::seconds;
using std::chrono::minutes;

/**
 * A tow shorter than this was a winch launch; an aerotow takes
 * several minutes.
 */
static constexpr seconds MAX_WINCH_LAUNCH{120};

/**
 * An engine that runs this soon after the takeoff was the launch.
 */
static constexpr minutes MAX_SELF_LAUNCH_DELAY{3};

/**
 * Wait this long after the landing for the final distances before
 * writing the entry without them.
 */
static constexpr minutes MAX_FINAL_WAIT{2};

/**
 * A place name is looked for within these distances: an airfield
 * nearby first, then any waypoint (an outlanding field).
 */
static constexpr double LANDABLE_RANGE = 3000, WAYPOINT_RANGE = 1000;

GlueLogbook::GlueLogbook(LiveBlackboard &_blackboard, Path _path,
                         const Waypoints *_waypoints,
                         const Logger *_igc_logger) noexcept
  :blackboard(_blackboard), waypoints(_waypoints), igc_logger(_igc_logger),
   path(_path)
{
  blackboard.AddListener(*this);
}

GlueLogbook::~GlueLogbook() noexcept
{
  blackboard.RemoveListener(*this);
}

/**
 * The UTC date and time of an event of the calculation thread, from
 * the current GPS fix.
 */
[[gnu::pure]]
static BrokenDateTime
ToDateTime(const MoreData &basic, TimeStamp t) noexcept
{
  if (!t.IsDefined())
    return basic.date_time_utc;

  const auto ago = std::chrono::duration_cast<seconds>(basic.time - t);
  return basic.date_time_utc - ago;
}

/**
 * A waypoint from a file names a place; a temporary one does not,
 * e.g. "(takeoff)", which the task manager creates at the takeoff.
 */
[[gnu::pure]]
static bool
IsPlace(const Waypoint &wp) noexcept
{
  return wp.origin != WaypointOrigin::NONE;
}

[[gnu::pure]]
static bool
IsLandablePlace(const Waypoint &wp) noexcept
{
  return wp.IsLandable() && IsPlace(wp);
}

std::string
GlueLogbook::FindPlace(const GeoPoint &location) const noexcept
{
  if (!location.IsValid())
    return {};

  if (waypoints != nullptr) {
    auto wp = waypoints->GetNearestIf(location, LANDABLE_RANGE,
                                      IsLandablePlace);
    if (wp == nullptr)
      wp = waypoints->GetNearestIf(location, WAYPOINT_RANGE, IsPlace);
    if (wp != nullptr)
      return wp->name;
  }

  /* no waypoint there: the coordinates, in the format the pilot
     reads elsewhere */
  return FormatGeoPoint(location,
                        blackboard.GetUISettings().format.coordinate_format)
    .c_str();
}

void
GlueLogbook::OnTakeoff(const MoreData &basic, const DerivedInfo &calculated)
{
  const FlyingState &flight = calculated.flight;
  const ComputerSettings &settings = blackboard.GetComputerSettings();

  entry = {};
  entry.takeoff = ToDateTime(basic, flight.takeoff_time);
  entry.takeoff_place = FindPlace(flight.takeoff_location);
  entry.pilot = settings.logger.pilot_name.c_str();
  entry.copilot = settings.logger.copilot_name.c_str();
  entry.aircraft = settings.plane.type.c_str();
  entry.registration = settings.plane.registration.c_str();
  entry.competition_id = settings.plane.competition_id.c_str();

  takeoff_time = flight.takeoff_time;
  engine_at_launch = false;
  in_flight = true;
  landed = false;
}

void
GlueLogbook::OnFlying(const MoreData &basic, const DerivedInfo &calculated)
{
  if (basic.NavAltitudeAvailable())
    entry.max_altitude = std::max(entry.max_altitude,
                                  (int)std::lround(basic.nav_altitude));

  /* the logger may start a little after the takeoff; remember the
     file it writes */
  if (igc_logger != nullptr && entry.igc_file.empty())
    if (const auto igc = igc_logger->GetActivePath(); igc != nullptr)
      entry.igc_file = igc.GetBase().c_str();

  const FlyingState &flight = calculated.flight;
  if (flight.power_on_time.IsDefined() && takeoff_time.IsDefined() &&
      flight.power_on_time - takeoff_time < MAX_SELF_LAUNCH_DELAY)
    engine_at_launch = true;
}

void
GlueLogbook::OnLanding(const MoreData &basic, const DerivedInfo &calculated)
{
  const FlyingState &flight = calculated.flight;

  entry.landing = ToDateTime(basic, flight.landing_time);
  entry.landing_place = FindPlace(flight.landing_location);

  if (engine_at_launch)
    entry.launch = LogbookEntry::Launch::SELF;
  else if (flight.release_time.IsDefined() && takeoff_time.IsDefined())
    entry.launch = flight.release_time - takeoff_time <= MAX_WINCH_LAUNCH
      ? LogbookEntry::Launch::WINCH
      : LogbookEntry::Launch::AEROTOW;

  landed = true;
  landed_at = basic.time;
}

void
GlueLogbook::Finish(const DerivedInfo &calculated)
{
  const LogbookStatistics &stats = calculated.logbook_stats;
  entry.free_distance = stats.free.distance;
  entry.dmst_distance = stats.dmst.distance;
  entry.dmst_points = stats.dmst.score;
  entry.dmst_shape = stats.dmst_shape;

  try {
    Logbook::Append(path, entry);
  } catch (...) {
    LogError(std::current_exception(), "Failed to write the log book");
  }

  in_flight = false;
  landed = false;
}

void
GlueLogbook::OnCalculatedUpdate(const MoreData &basic,
                                const DerivedInfo &calculated)
{
  /* like flights.log: a replay or the simulator is not a flight of
     the pilot */
  if (basic.gps.replay || basic.gps.simulator)
    return;

  if (!basic.time_available || !basic.date_time_utc.IsDatePlausible())
    return;

  const FlyingState &flight = calculated.flight;

  if (!in_flight) {
    /* a flight begins once it is confirmed (in the air, not just
       rolling) */
    if (flight.flying && !flight.on_ground)
      OnTakeoff(basic, calculated);
    else
      return;
  }

  if (!landed) {
    if (flight.flying) {
      OnFlying(basic, calculated);
      return;
    }

    if (!flight.landing_time.IsDefined())
      return;

    OnLanding(basic, calculated);
  }

  /* landed: the calculation thread searches the distances once more
     after the landing; wait for that, but not for ever */
  if (calculated.logbook_stats.final ||
      basic.time - landed_at >= MAX_FINAL_WAIT ||
      basic.time < landed_at)
    Finish(calculated);
}
