// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <cstdint>
#include <optional>
#include <string>

class Waypoints;
struct GeoPoint;
enum class CoordinateFormat : uint8_t;

namespace Logbook {

/**
 * The name the log book gives a takeoff or landing place: the nearest
 * airfield within 3 km, else the nearest waypoint within 1 km, else
 * the coordinates.  Temporary waypoints such as "(takeoff)" do not
 * name a place.
 *
 * @param waypoints the waypoint database, may be nullptr
 */
[[gnu::pure]]
std::string
FindPlace(const Waypoints *waypoints, const GeoPoint &location,
          CoordinateFormat format) noexcept;

/**
 * The elevation of the nearest airfield within 3 km that has one, to
 * tell a takeoff from the ground from a recording that began in the
 * air near the airfield.
 *
 * @param waypoints the waypoint database, may be nullptr
 */
[[gnu::pure]]
std::optional<double>
FindAirfieldElevation(const Waypoints *waypoints,
                      const GeoPoint &location) noexcept;

} // namespace Logbook
