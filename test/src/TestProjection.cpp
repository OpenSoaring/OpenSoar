// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Projection/Projection.hpp"
#include "TestUtil.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

static void
TestGeoScreenCouple(const Projection prj, const GeoPoint geo,
                    int x, int y)
{
  auto tmp_pt = prj.GeoToScreen(geo);
  ok1(tmp_pt.x == x);
  ok1(tmp_pt.y == y);

  GeoPoint tmp_geo = prj.ScreenToGeo({x, y});
  ok1(equals(tmp_geo.latitude, geo.latitude));
  ok1(equals(tmp_geo.longitude, geo.longitude));
}

static void
test_simple()
{
  Projection prj;
  prj.SetGeoLocation(GeoPoint::Zero());

  TestGeoScreenCouple(prj, GeoPoint(Angle::Zero(),
                                    Angle::Zero()), 0, 0);
}

/**
 * Converting a point to the screen and back must give the point again
 * (up to the pixel rounding), with and without screen rotation.
 */
static void
test_round_trip(Angle screen_angle)
{
  Projection prj;
  prj.SetScreenOrigin(400, 240);
  prj.SetScreenAngle(screen_angle);
  prj.SetGeoLocation(GeoPoint(Angle::Degrees(7.7), Angle::Degrees(51.05)));
  prj.SetScale(800. / 100000.); // 100 km across 800 pixels

  const GeoPoint points[] = {
    GeoPoint(Angle::Degrees(7.7), Angle::Degrees(51.05)),
    GeoPoint(Angle::Degrees(8.3), Angle::Degrees(51.4)),
    GeoPoint(Angle::Degrees(7.1), Angle::Degrees(50.6)),
  };

  /* one pixel is 125 m here, i.e. about 0.0011 degrees latitude */
  for (const auto &g : points) {
    const auto back = prj.ScreenToGeo(prj.GeoToScreen(g));
    ok1(fabs((back.latitude - g.latitude).Degrees()) < 0.002);
    ok1(fabs((back.longitude - g.longitude).AsDelta().Degrees()) < 0.004);
  }
}

/**
 * At an integer zoom level, a slippy map tile must cover exactly 256 by
 * 256 screen pixels, wherever it is on the screen; that is the point of
 * drawing the map in Web Mercator.
 */
static void
test_tile_pixels()
{
  constexpr unsigned zoom = 12;
  constexpr unsigned n = 1u << zoom;

  const auto tile_longitude = [](unsigned x) {
    return Angle::Degrees(x * 360. / n - 180.);
  };
  const auto tile_latitude = [](unsigned y) {
    return Angle::Radians(atan(sinh(M_PI * (1 - 2. * y / n))));
  };

  Projection prj;
  prj.SetScreenOrigin(0, 0);
  prj.SetGeoLocation(GeoPoint(Angle::Degrees(7.7), Angle::Degrees(51.05)));
  prj.SetZoomLevel(zoom);

  ok1(fabs(prj.GetZoomLevel() - zoom) < 1e-9);

  /* tiles near the center and two tiles (about 30 km) further north */
  for (unsigned y : {1370u, 1371u, 1375u}) {
    for (unsigned x : {2135u, 2137u}) {
      const auto nw = prj.GeoToScreen(GeoPoint(tile_longitude(x),
                                               tile_latitude(y)));
      const auto se = prj.GeoToScreen(GeoPoint(tile_longitude(x + 1),
                                               tile_latitude(y + 1)));
      ok1(se.x - nw.x == 256);
      ok1(se.y - nw.y == 256);
    }
  }
}

/**
 * GeoToScreen() takes the Mercator y from a Taylor series near the
 * location; it must agree with the exact formula, also far north and
 * at the closest zoom level.
 */
static void
test_series(double latitude, double zoom)
{
  Projection prj;
  prj.SetScreenOrigin(0, 0);
  prj.SetGeoLocation(GeoPoint(Angle::Degrees(10), Angle::Degrees(latitude)));
  prj.SetZoomLevel(zoom);

  const double pixels_per_radian = 256 * exp2(zoom) / (2 * M_PI);
  const double y0 = asinh(tan(latitude * M_PI / 180));

  int max_error = 0;
  for (int k = -200; k <= 200; ++k) {
    const double lat = latitude + k * 0.01;
    const double exact = -(asinh(tan(lat * M_PI / 180)) - y0)
      * pixels_per_radian;
    if (fabs(exact) > 1e6)
      /* FastIntegerRotation works in fixed point with 10 fraction bits
         and overflows beyond 2^21 pixels; such points never occur on
         a real screen */
      continue;

    const int y = prj.GeoToScreen(GeoPoint(Angle::Degrees(10),
                                           Angle::Degrees(lat))).y;
    max_error = std::max(max_error, abs(y - int(lround(exact))));
  }

  ok1(max_error <= 1);
}

/**
 * SetScale() is the ground scale at the location: one kilometer north
 * must be scale*1000 pixels.
 */
static void
test_ground_scale()
{
  Projection prj;
  prj.SetScreenOrigin(0, 0);
  prj.SetGeoLocation(GeoPoint(Angle::Degrees(10), Angle::Degrees(60)));
  prj.SetScale(1. / 10.); // 10 m per pixel

  /* 1000 m north along the meridian, on the FAI sphere */
  const GeoPoint north(Angle::Degrees(10),
                       Angle::Degrees(60) + Angle::Radians(1000. / 6371000.));
  ok1(prj.GeoToScreen(north).y == -100);

  /* 1000 m east along the parallel */
  const GeoPoint east(Angle::Degrees(10) +
                      Angle::Radians(1000. / (6371000. * cos(M_PI / 3))),
                      Angle::Degrees(60));
  ok1(prj.GeoToScreen(east).x == 100);
}

int main()
{
  plan_tests(4 + 2 * 6 + 1 + 12 + 6 + 2);

  test_simple();
  test_round_trip(Angle::Zero());
  test_round_trip(Angle::Degrees(37));
  test_tile_pixels();
  test_series(0, 19);
  test_series(50, 19);
  test_series(50, 8);
  test_series(68, 19);
  test_series(78, 19);
  test_series(78, 12);
  test_ground_scale();

  return exit_status();
}
