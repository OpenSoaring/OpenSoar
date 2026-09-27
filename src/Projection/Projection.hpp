// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Geo/GeoPoint.hpp"
#include "Math/FastRotation.hpp"
#include "Math/Util.hpp"
#include "ui/dim/Point.hpp"

#include <cassert>

/**
 * This is a class that can be used for converting geographical into screen
 * coordinates and vice-versa.
 *
 * The map is drawn in the spherical ("Web") Mercator projection, the
 * same projection that slippy map tiles are rendered in.  Raster tile
 * overlays can then be placed as undistorted rectangles, and at the
 * matching zoom level (see SetZoomLevel()) one tile pixel lands on
 * exactly one screen pixel.  The scale set with SetScale() is the true
 * ground scale at the GeoLocation; away from it, the Mercator scale
 * factor applies, which is negligible within the area of one screen.
 *
 * For doing so one needs to at least set a scaling factor (m/px) by calling
 * the SetScale() function.
 *
 *  Optional features
 * -------------------
 *
 * ScreenOrigin: By calling SetScreenOrigin the screen origin offset
 * can be set. This is the offset that the screen coordinates will be shifted
 * in the conversion functions. It is also the point of rotation for the
 * ScreenRotation.
 *
 * GeoLocation: By calling the SetGeoLocation() function the ScreenOrigin can
 * be mapped to a new geographical location, which will be taken into account
 * when converting other coordinates.
 *
 * ScreenRotation: By calling SetScreenAngle() the rotation angle for the
 * conversions can be set.
 */
class Projection
{
  /** This is the geographical location that the ScreenOrigin is mapped to */
  GeoPoint geo_location = GeoPoint::Invalid();

  /**
   * This is the point that the ScreenRotation will rotate around.
   * It is also the point that the GeoLocation points to.
   */
  PixelPoint screen_origin = {0, 0};

  Angle screen_angle = Angle::Zero();

  /**
   * FastIntegerRotation instance for fast
   * rotation in the conversion functions
   */
  FastIntegerRotation screen_rotation;

  /** The earth's radius in screen coordinates (px) */
  double draw_scale;
  /** Inverted value of DrawScale for faster calculations */
  double inv_draw_scale;

  /** This is the scaling factor in px/m */
  double scale;

  /**
   * The Mercator y coordinate of #geo_location, cached because every
   * conversion needs it.
   */
  double mercator_y = 0;

  /**
   * Screen pixels per Mercator radian.  This is #draw_scale times the
   * cosine of the latitude of #geo_location, because the Mercator
   * projection stretches everything by 1/cos(latitude) and #scale is
   * meant as the ground scale at #geo_location.
   */
  double mercator_scale = 1;

  /**
   * Coefficients of the Taylor series of the Mercator y around the
   * latitude of #geo_location, already multiplied by
   * #mercator_scale.  asinh(tan(latitude)) costs several times more
   * than the whole rest of GeoToScreen(), and GeoToScreen() runs for
   * every airspace vertex and trail point in every frame; near the
   * location, four terms are exact to a small fraction of a pixel.
   */
  double mercator_series[4] = {};

  /**
   * The largest latitude difference (radians) from #geo_location for
   * which #mercator_series is used; see UpdateMercatorSeries().
   */
  double mercator_series_limit = 0;

  void UpdateMercator() noexcept;
  void UpdateMercatorSeries() noexcept;

  [[gnu::pure]]
  double LatitudeToScreenY(Angle latitude) const noexcept;

public:
  Projection() noexcept;

  bool IsValid() const noexcept {
    return geo_location.IsValid();
  }

  [[gnu::pure]]
  double GetScale() const noexcept {
    return scale;
  }

  /**
   * Sets the scaling factor
   * @param _scale New scale in px/m
   */
  void SetScale(double _scale) noexcept;

  /**
   * Convert a pixel distance to a physical length in meters.
   */
  [[gnu::pure]]
  double DistancePixelsToMeters(const int x) const noexcept {
    return double(x) / GetScale();
  }

  [[gnu::pure]]
  double DistanceMetersToPixels(const double distance) const noexcept {
    return distance * GetScale();
  }

  /**
   * Convert a pixel distance to an angle on Earth's surface.
   */
  [[gnu::pure]]
  Angle PixelsToAngle(int pixels) const noexcept {
    return Angle::Radians(pixels * inv_draw_scale);
  }

  /**
   * Convert a an angle on Earth's surface to a pixel distance.
   */
  [[gnu::pure]]
  double AngleToPixels(Angle angle) const noexcept {
    return angle.Radians() * draw_scale;
  }

  /**
   * Converts screen coordinates to a GeoPoint
   */
  [[gnu::pure]]
  GeoPoint ScreenToGeo(PixelPoint p) const noexcept;

  /**
   * Converts a GeoPoint to screen coordinates
   * @param g GeoPoint to convert
   */
  [[gnu::pure]]
  PixelPoint GeoToScreen(const GeoPoint &g) const noexcept;

  /**
   * Returns the origin/rotation center in screen coordinates
   * @return The origin/rotation center in screen coordinates
   */
  const PixelPoint &GetScreenOrigin() const noexcept {
    return screen_origin;
  }

  /**
   * Set the origin/rotation center to the given screen coordinates
   * @param x Screen coordinate in x-direction
   * @param y Screen coordinate in y-direction
   */
  void SetScreenOrigin(int x, int y) noexcept {
    screen_origin.x = x;
    screen_origin.y = y;
  }

  /**
   * Set the origin/rotation center to the given screen coordinates
   * @param pt Screen coordinate
   */
  void SetScreenOrigin(PixelPoint pt) noexcept {
    screen_origin = pt;
  }

  /**
   * Returns the GeoPoint at the ScreenOrigin
   * @return GeoPoint at the ScreenOrigin
   */
  const GeoPoint &GetGeoLocation() const noexcept {
    assert(IsValid());

    return geo_location;
  }

  /**
   * Set the GeoPoint that relates to the ScreenOrigin
   * @param g The new GeoPoint
   */
  void SetGeoLocation(GeoPoint g) noexcept {
    geo_location = g;
    geo_location.Normalize();
    UpdateMercator();
  }

  /**
   * Returns the number of screen pixels per Mercator radian.
   */
  [[gnu::pure]]
  double GetMercatorScale() const noexcept {
    return mercator_scale;
  }

  /**
   * Returns the slippy map zoom level that matches the current scale
   * at the current GeoLocation.  An integer value means that tiles of
   * that level are shown with one tile pixel per screen pixel.
   */
  [[gnu::pure]]
  double GetZoomLevel() const noexcept;

  /**
   * Sets the scale so that tiles of the given zoom level are shown
   * with one tile pixel per screen pixel at the current GeoLocation.
   * The GeoLocation must be set first, because the ground scale
   * belonging to a zoom level depends on the latitude.
   */
  void SetZoomLevel(double zoom) noexcept;

  /**
   * Converts a geographical distance (m) to a screen distance (px)
   * @param x A geographical distance (m)
   * @return The converted distance in px
   */
  unsigned GeoToScreenDistance(const double x) const noexcept {
    return uround(scale * x);
  }

  /**
   * Returns the current screen rotation angle
   * @return Screen rotation angle
   */
  Angle GetScreenAngle() const noexcept {
    return screen_angle;
  }

  /**
   * Sets the screen rotation angle
   * @param angle New screen rotation angle
   */
  void SetScreenAngle(Angle angle) noexcept {
    screen_rotation = screen_angle = angle;
  }

  /**
   * Creates a FastRowRotation object base on the current screen
   * rotation angle and the specified screen row.
   */
  FastRowRotation GetScreenAngleRotation(int y) const noexcept {
    return FastRowRotation(screen_rotation, y);
  }
};
