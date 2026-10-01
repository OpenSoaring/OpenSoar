// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LogbookPlace.hpp"
#include "Engine/Waypoint/Waypoints.hpp"
#include "Engine/Waypoint/Waypoint.hpp"
#include "Formatter/GeoPointFormatter.hpp"
#include "Geo/GeoPoint.hpp"

/**
 * A place name is looked for within these distances: an airfield
 * nearby first, then any waypoint (an outlanding field).
 */
static constexpr double LANDABLE_RANGE = 3000, WAYPOINT_RANGE = 1000;

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

namespace Logbook {

std::string
FindPlace(const Waypoints *waypoints, const GeoPoint &location,
          CoordinateFormat format) noexcept
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
  return FormatGeoPoint(location, format).c_str();
}

} // namespace Logbook
