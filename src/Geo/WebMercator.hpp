// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Math/Angle.hpp"

#include <algorithm>
#include <cmath>

/**
 * Helpers for the spherical ("Web") Mercator projection, the
 * projection that slippy map tiles (OpenStreetMap, MBTiles, SkySight
 * live tiles) are rendered in.  The map is drawn in the same
 * projection so that a tile can be put on the screen as a plain
 * rectangle, pixel for pixel, instead of being resampled into a
 * differently shaped area.
 *
 * All Mercator coordinates here are in radians of the unit sphere:
 * x is the longitude, y is asinh(tan(latitude)).  A whole zoom level 0
 * tile therefore spans 2*pi in both directions.
 */
namespace WebMercator {

/**
 * The latitude at which the Mercator square of the tile pyramid ends;
 * beyond it, tile sources have no data and y grows without bound.
 */
static constexpr double MAX_LATITUDE_DEGREES = 85.0511287798066;

/** The y value that belongs to #MAX_LATITUDE_DEGREES, which is pi. */
static constexpr double MAX_Y = M_PI;

/** The edge length of one tile in pixels. */
static constexpr double TILE_SIZE = 256;

[[gnu::const]]
static inline double
LatitudeToY(Angle latitude) noexcept
{
  const double max = MAX_LATITUDE_DEGREES * M_PI / 180.;
  const double phi = std::clamp(latitude.Radians(), -max, max);
  return std::asinh(std::tan(phi));
}

[[gnu::const]]
static inline Angle
YToLatitude(double y) noexcept
{
  return Angle::Radians(std::atan(std::sinh(std::clamp(y, -MAX_Y, MAX_Y))));
}

/**
 * The number of screen pixels per Mercator radian at which one tile
 * pixel of the given zoom level covers exactly one screen pixel.
 */
[[gnu::const]]
static inline double
ZoomToPixelsPerRadian(double zoom) noexcept
{
  return TILE_SIZE * std::exp2(zoom) / (2 * M_PI);
}

[[gnu::const]]
static inline double
PixelsPerRadianToZoom(double pixels_per_radian) noexcept
{
  return std::log2(pixels_per_radian * (2 * M_PI) / TILE_SIZE);
}

} // namespace WebMercator
