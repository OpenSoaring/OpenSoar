// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "MapWindowProjection.hpp"
#include "Screen/Layout.hpp"
#include "Waypoint/Waypoint.hpp"
#include "Geo/FAISphere.hpp"
#include "Geo/WebMercator.hpp"

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Globals.hpp"
#endif

#include <algorithm> // for std::clamp()
#include <cassert>
#include <cmath>

/**
 * The zoom levels that the map snaps to, from the closest to the
 * widest view.  They are slippy map zoom levels, one factor of two
 * apart: at each of them, a raster tile of that level is shown with
 * one tile pixel per screen pixel (see Projection::SetZoomLevel()),
 * so tile overlays stay sharp.  Level 19 shows roughly 150 m, level
 * 6 roughly 1250 km across an 800 pixel wide screen at 50 degrees
 * latitude; the ground distance of a level depends on the latitude,
 * which is why the list cannot be expressed in meters.
 */
static constexpr unsigned ScaleList[] = {
  19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6,
};

static constexpr unsigned ScaleListCount = std::size(ScaleList);

namespace {

/**
 * Largest #GetMapScale() at which this waypoint is still drawn (aligned with
 * #ScaleList steps; lower threshold = hidden sooner when zooming out).
 */
static double
WaypointDrawMaxScale(const Waypoint &wp) noexcept
{
  if (wp.IsLandable())
    return 20000;
  if (wp.type == Waypoint::Type::OBSTACLE)
    return 5000;
  return 10000;
}

} // namespace

bool
MapWindowProjection::WaypointInScaleFilter(const Waypoint &way_point) const noexcept
{
  return GetMapScale() <= WaypointDrawMaxScale(way_point);
}

/**
 * A free scale closer than this (in zoom levels) to one of the
 * #ScaleList levels snaps to it; 0.03 levels are about 2% of scale,
 * which the eye does not notice, but it keeps pinch and animated zoom
 * ending exactly on a level where the tiles are sharp.
 */
static constexpr double SNAP_ZOOM_TOLERANCE = 0.03;

double
MapWindowProjection::GetLatitudeCosine() const noexcept
{
  /* without a location, the equator is as good a guess as any; the
     scale is recalculated when the location arrives */
  return IsValid()
    ? std::cos(GetGeoLocation().latitude.Radians())
    : 1.;
}

double
MapWindowProjection::ZoomToMapScale(double zoom) const noexcept
{
  /* map scale = meters per GetMapResolutionFactor() pixels */
  const double pixels_per_meter = WebMercator::ZoomToPixelsPerRadian(zoom)
    / (FAISphere::REARTH * GetLatitudeCosine());
  return GetMapResolutionFactor() / pixels_per_meter;
}

double
MapWindowProjection::MapScaleToZoom(double map_scale) const noexcept
{
  const double pixels_per_meter = GetMapResolutionFactor() / map_scale;
  return WebMercator::PixelsPerRadianToZoom(pixels_per_meter *
                                            FAISphere::REARTH *
                                            GetLatitudeCosine());
}

double
MapWindowProjection::CalculateMapScale(unsigned scale) const noexcept
{
  assert(scale < ScaleListCount);
  return ZoomToMapScale(ScaleList[scale]);
}

double
MapWindowProjection::GetMaxMapScale() const noexcept
{
#ifdef ENABLE_OPENGL
  /* OpenGL::max_map_scale was chosen as an entry of the former scale
     list, which turned a value v into the map scale
     v * GetMapResolutionFactor() / Layout::Scale(screen width); the
     ground distance across the screen was v * 240 / (shorter screen
     side in pixels), so 502000 for the Mali-400 meant about 250 km on
     an 800x480 screen, and 300000 for the PowerVR GE8300 about 120 km
     on a 1024x600 one.  The limit is converted the same way, so that
     the zoom levels end where the old list ended.  The raw value taken
     as a map scale would be thousands of kilometers across the screen
     and would never stop at any of the zoom levels. */
  const unsigned width = GetScreenSize().width;
  if (OpenGL::max_map_scale > 0 && width > 0)
    return double(OpenGL::max_map_scale) * GetMapResolutionFactor()
      / Layout::Scale(width);
#endif

  return 0;
}

/**
 * Determine the effective number of usable entries in the ScaleList.
 * May be reduced by OpenGL::max_map_scale to work around GPU driver
 * bugs.
 */
unsigned
MapWindowProjection::EffectiveScaleListCount() const noexcept
{
  if (const double max_map_scale = GetMaxMapScale(); max_map_scale > 0) {
    for (unsigned i = 0; i < ScaleListCount; i++)
      if (CalculateMapScale(i) > max_map_scale)
        return std::max(i, 1u);
  }

  return ScaleListCount;
}

double
MapWindowProjection::LimitMapScale(const double value) const noexcept
{
  return HaveScaleList() ? CalculateMapScale(FindMapScale(value)) : value;
}

double
MapWindowProjection::StepMapScale(const double scale, int Step) const noexcept
{
  int i = FindMapScale(scale) + Step;
  i = std::clamp(i, 0, (int)EffectiveScaleListCount() - 1);
  return CalculateMapScale(i);
}

unsigned
MapWindowProjection::FindMapScale(const double Value) const noexcept
{
  const unsigned effective_count = EffectiveScaleListCount();

  /* the levels are one factor of two apart, so the nearest one is
     found by rounding the zoom level, which is logarithmic */
  const double zoom = MapScaleToZoom(Value);

  unsigned best = 0;
  for (unsigned i = 1; i < effective_count; i++)
    if (std::fabs(ScaleList[i] - zoom) < std::fabs(ScaleList[best] - zoom))
      best = i;

  return best;
}

void
MapWindowProjection::SnapToZoomLevel(unsigned zoom) noexcept
{
  snapped_zoom = zoom;

  if (IsValid())
    SetZoomLevel(zoom);
  else
    Projection::SetScale(GetMapResolutionFactor() / ZoomToMapScale(zoom));
}

void
MapWindowProjection::ApplyScale(double pixels_per_meter) noexcept
{
  const double map_scale = GetMapResolutionFactor() / pixels_per_meter;
  const unsigned i = FindMapScale(map_scale);
  if (std::fabs(MapScaleToZoom(map_scale) - ScaleList[i]) <
      SNAP_ZOOM_TOLERANCE) {
    SnapToZoomLevel(ScaleList[i]);
    return;
  }

  snapped_zoom = 0;
  Projection::SetScale(pixels_per_meter);
}

void
MapWindowProjection::SetScale(double pixels_per_meter) noexcept
{
  ApplyScale(pixels_per_meter);
}

void
MapWindowProjection::SetGeoLocation(GeoPoint g) noexcept
{
  WindowProjection::SetGeoLocation(g);

  /* the ground scale of a zoom level changes with the latitude; keep
     the zoom level, not the ground scale, so that tiles stay sharp
     while the aircraft moves north or south */
  if (snapped_zoom > 0 && IsValid())
    SetZoomLevel(snapped_zoom);
}

void
MapWindowProjection::SetFreeMapScale(double x) noexcept
{
  /* the same limit as for the zoom levels; pinching, auto zoom and the
     zoom animation must not get past it either */
  if (const double max_map_scale = GetMaxMapScale(); max_map_scale > 0)
    x = std::min(x, max_map_scale);

  ApplyScale(double(GetMapResolutionFactor()) / x);
}

void
MapWindowProjection::SetMapScale(const double x) noexcept
{
  SnapToZoomLevel(ScaleList[FindMapScale(x)]);
}
