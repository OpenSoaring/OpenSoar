// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Projection.hpp"
#include "Geo/FAISphere.hpp"
#include "Geo/WebMercator.hpp"
#include "Math/Angle.hpp"

#include <algorithm>
#include <cmath>

Projection::Projection() noexcept
{
  SetScale(1);
}

GeoPoint
Projection::ScreenToGeo(PixelPoint src) const noexcept
{
  assert(IsValid());

  /* after rotation, x points east and y points south, both in screen
     pixels relative to the GeoLocation */
  const auto p =
    screen_rotation.Rotate(src - screen_origin);

  const double inv_mercator_scale = 1. / mercator_scale;

  GeoPoint g;
  g.longitude = geo_location.longitude +
    Angle::Radians(p.x * inv_mercator_scale);

  /* YToLatitude() clamps to the edge of the Mercator square; this
     also keeps panning all the way up/down from producing
     meaningless latitudes */
  g.latitude = WebMercator::YToLatitude(mercator_y -
                                        p.y * inv_mercator_scale);

  /* Keep longitude in (-180°, 180°]; unnormalized values (e.g. -188°)
     break GeoBounds wrap detection and cartesian overlay clipping. */
  return g.Normalize();
}

PixelPoint
Projection::GeoToScreen(const GeoPoint &g) const noexcept
{
  assert(IsValid());

  /* x is measured westwards and y southwards here, which is what the
     rotation and the sign flip below expect */
  const double dx = (geo_location.longitude - g.longitude).AsDelta().Radians()
    * mercator_scale;
  const double dy = LatitudeToScreenY(g.latitude);

  /* round instead of truncating: truncation pulls every point towards
     the GeoLocation, which opens one pixel wide gaps between adjacent
     tiles on the far side */
  const auto p =
    screen_rotation.Rotate(PixelPoint(iround(dx), iround(dy)));

  PixelPoint sc;
  sc.x = screen_origin.x - p.x;
  sc.y = screen_origin.y + p.y;
  return sc;
}

void
Projection::SetScale(const double _scale) noexcept
{
  scale = _scale;

  // Calculate earth radius in pixels
  draw_scale = FAISphere::REARTH * scale;
  // Save inverted value for faster calculations
  inv_draw_scale = 1. / draw_scale;

  UpdateMercator();
}

void
Projection::UpdateMercator() noexcept
{
  if (!IsValid()) {
    /* without a location there is no latitude to take the Mercator
       stretch from; the values are recalculated as soon as the
       location is set */
    mercator_y = 0;
    mercator_scale = draw_scale;
    return;
  }

  mercator_y = WebMercator::LatitudeToY(geo_location.latitude);
  mercator_scale = draw_scale * std::cos(geo_location.latitude.Radians());
  UpdateMercatorSeries();
}

void
Projection::UpdateMercatorSeries() noexcept
{
  /* derivatives of y = asinh(tan(phi)): sec, sec*tan,
     sec*(2*tan^2+1), sec*tan*(6*tan^2+5); divided by n! */
  const double phi = geo_location.latitude.Radians();
  const double sec = 1. / std::cos(phi);
  const double tan = std::tan(phi);
  const double tan2 = tan * tan;

  mercator_series[0] = mercator_scale * sec;
  mercator_series[1] = mercator_scale * sec * tan / 2;
  mercator_series[2] = mercator_scale * sec * (2 * tan2 + 1) / 6;
  mercator_series[3] = mercator_scale * sec * tan * (6 * tan2 + 5) / 24;

  /* the first omitted term is y5/120 * d^5 with y5 =
     sec*(24*tan^4+28*tan^2+5); choose the range so that it stays below
     1/50 pixel at the current scale.  It shrinks towards the poles and
     at close zoom, and is capped at one degree, beyond which points are
     far off the screen and the exact formula is affordable. */
  constexpr double max_error_pixels = 0.02;
  const double y5 = sec * (24 * tan2 * tan2 + 28 * tan2 + 5);
  mercator_series_limit =
    std::min(M_PI / 180,
             std::pow(max_error_pixels * 120 / (y5 * mercator_scale), 0.2));

  /* beyond 80 degrees the location is near the clamped edge of the
     Mercator square, where the series would not match the clamping */
  if (std::fabs(phi) > 80 * M_PI / 180)
    mercator_series_limit = 0;
}

double
Projection::LatitudeToScreenY(Angle latitude) const noexcept
{
  /* returns the distance south of the location in pixels */
  const double d = (latitude - geo_location.latitude).Radians();

  /* the negated comparison also takes NaN to the exact formula */
  if (!(std::fabs(d) <= mercator_series_limit))
    return (mercator_y - WebMercator::LatitudeToY(latitude)) * mercator_scale;

  const auto &c = mercator_series;
  return -d * (c[0] + d * (c[1] + d * (c[2] + d * c[3])));
}

double
Projection::GetZoomLevel() const noexcept
{
  return WebMercator::PixelsPerRadianToZoom(mercator_scale);
}

void
Projection::SetZoomLevel(double zoom) noexcept
{
  assert(IsValid());

  const double pixels_per_radian = WebMercator::ZoomToPixelsPerRadian(zoom);
  SetScale(pixels_per_radian /
           (FAISphere::REARTH * std::cos(geo_location.latitude.Radians())));

  /* SetScale() derived mercator_scale by multiplying with the cosine
     again; set it directly so that it is exact and tiles of this zoom
     level really land pixel for pixel */
  mercator_scale = pixels_per_radian;
  UpdateMercatorSeries();
}
