// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "WindowProjection.hpp"

struct Waypoint;

class MapWindowProjection:
  public WindowProjection
{
  /**
   * The zoom level the map is locked to, or 0 if the scale is free.
   * While locked, a new location keeps the zoom level instead of the
   * ground scale, see SetGeoLocation().
   */
  unsigned snapped_zoom = 0;

  /**
   * A scale was set before the location was known, so whether it is
   * close to a zoom level could not be decided yet (the ground scale
   * of a level depends on the latitude); SetGeoLocation() checks
   * again.
   */
  bool snap_pending = false;

public:
  /**
   * Sets the scale in px/m.  Hides Projection::SetScale(): a scale
   * close to one of the zoom levels snaps to it, so that a scale
   * restored from the profile keeps tile overlays sharp.
   */
  void SetScale(double pixels_per_meter) noexcept;

  /**
   * Hides Projection::SetGeoLocation(): while the scale is locked to a
   * zoom level, the ground scale follows the latitude.
   */
  void SetGeoLocation(GeoPoint g) noexcept;

  /**
   * Sets a map scale which is not affected by the hard-coded scale
   * list.
   */
  void SetFreeMapScale(double x) noexcept;

  void SetMapScale(double x) noexcept;

public:
  bool HaveScaleList() const noexcept {
    return true;
  }

  /**
   * Calculates a scale index.
   */
  [[gnu::pure]]
  double CalculateMapScale(unsigned scale) const noexcept;

  [[gnu::pure]]
  double StepMapScale(double scale, int Step) const noexcept;

  /** Current map scale within draw band for this waypoint type (#GetMapScale units). */
  [[gnu::pure]]
  bool WaypointInScaleFilter(const Waypoint &way_point) const noexcept;

private:
  [[gnu::pure]]
  double GetLatitudeCosine() const noexcept;

  [[gnu::pure]]
  double ZoomToMapScale(double zoom) const noexcept;

  [[gnu::pure]]
  double MapScaleToZoom(double map_scale) const noexcept;

  /**
   * The largest map scale the GPU driver can cope with
   * (OpenGL::max_map_scale converted to #GetMapScale() units), or 0
   * for no limit.
   */
  [[gnu::pure]]
  double GetMaxMapScale() const noexcept;

  [[gnu::pure]]
  unsigned EffectiveScaleListCount() const noexcept;

  void SnapToZoomLevel(unsigned zoom) noexcept;
  void ApplyScale(double pixels_per_meter) noexcept;

  double LimitMapScale(double value) const noexcept;

  [[gnu::pure]]
  unsigned FindMapScale(double Value) const noexcept;
};
