// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ui/canvas/Bitmap.hpp"
#include "GeoBitmap.hpp"
#include "UncompressedImage.hpp"
#include "Geo/Quadrilateral.hpp"
#include "system/Path.hpp"
#include "Geo/GeoBounds.hpp"
#include "MapWindow/MapWindow.hpp"

#include "Interface.hpp"
#include "NMEA/ExternalSettings.hpp"

#include "LogFile.hpp"

#ifdef USE_GEOTIFF
# include "LibTiff.hpp"
#endif

#include <string>
#include <vector>
#include <stdexcept>

#include <boost/algorithm/string.hpp>

/* code (C/C++) from https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
======================================================= */
static int lon2tilex(double lon, int z)
{
  return (int)(floor((lon + 180.0) / 360.0 * (1 << z)));
}

static int lat2tiley(double lat, int z)
{
  double latrad = lat * M_PI / 180.0;
  return (int)(floor((1.0 - asinh(tan(latrad)) / M_PI) / 2.0 * (1 << z)));
}

static double tilex2lon(int x, int z)
{
  return x / (double)(1 << z) * 360.0 - 180;
}

static double tiley2lat(int y, int z)
{
  double n = M_PI - 2.0 * M_PI * y / (double)(1 << z);
  return 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n)));
}

GeoBitmap::TileData
GeoBitmap::GetTile(const GeoBounds &bounds, const uint16_t zoom)
{
  GeoBitmap::TileData tile;
  tile.zoom = zoom;
  tile.x = lon2tilex(bounds.GetCenter().longitude.Degrees(), tile.zoom);
  tile.y = lat2tiley(bounds.GetCenter().latitude.Degrees(), tile.zoom);

  return tile;
}
GeoBitmap::TileData
GeoBitmap::GetTile(const MapWindowProjection &proj)
{
  double ErdUmfang = 42e6;  // = 42.000 km
  double diagonale = proj.GetScreenDistanceMeters();
  double t = ErdUmfang / proj.GetScreenDistanceMeters();
  double _log = floor(log2(t));
  uint16_t zoom = _log + 1;

  return GetTile(proj.GetScreenBounds(), zoom);
}

GeoQuadrilateral
GeoBitmap::GetGeoQuadrilateral(const TileData &tile)
{
  double longitude[2] = {
    tilex2lon(tile.x, tile.zoom),
    tilex2lon(tile.x + 1, tile.zoom) };
  double latitude[2] = {
    tiley2lat(tile.y, tile.zoom),
    tiley2lat(tile.y + 1, tile.zoom) };
  // bounds.top_left.longitude = Angle::Radians(longitude[0]);
  GeoQuadrilateral bounds;
  bounds.top_left.longitude = Angle::Degrees(longitude[0]);
  bounds.top_left.latitude = Angle::Degrees(latitude[0]);
  bounds.bottom_left.longitude = Angle::Degrees(longitude[0]);
  bounds.bottom_left.latitude = Angle::Degrees(latitude[1]);

  bounds.top_right.longitude = Angle::Degrees(longitude[1]);
  bounds.top_right.latitude = Angle::Degrees(latitude[0]);
  bounds.bottom_right.longitude = Angle::Degrees(longitude[1]);
  bounds.bottom_right.latitude = Angle::Degrees(latitude[1]);

  return bounds;
}

GeoBounds
GeoBitmap::GetBounds(const TileData &tile) {

  return GetGeoQuadrilateral(tile).GetBounds();
}

GeoQuadrilateral
Bitmap::SetTileKoordinates(std::string_view tile_string)
{
  GeoQuadrilateral bounds;

  std::vector<std::string> vec; //  = explode(tile_string, '-');
  boost::split(vec, tile_string, boost::is_any_of("-"));

  GeoBitmap::TileData tile = { 0, 1, 2 };
  tile.zoom = atol(vec[0].c_str());
  tile.x = atol(vec[1].c_str());
  tile.y = atol(vec[2].c_str());

  return GetGeoQuadrilateral(tile);
/*/
  double longitude[2] = {
    tilex2lon(tile.x, tile.zoom),
    tilex2lon(tile.x + 1, tile.zoom) };
  double latitude[2] = {
    tiley2lat(tile.y, tile.zoom),
    tiley2lat(tile.y + 1, tile.zoom) };
  // bounds.top_left.longitude = Angle::Radians(longitude[0]);
  bounds.top_left.longitude = Angle::Degrees(longitude[0]);
  bounds.top_left.latitude = Angle::Degrees(latitude[0]);
  bounds.bottom_left.longitude = Angle::Degrees(longitude[0]);
  bounds.bottom_left.latitude = Angle::Degrees(latitude[1]);

  bounds.top_right.longitude = Angle::Degrees(longitude[1]);
  bounds.top_right.latitude = Angle::Degrees(latitude[0]);
  bounds.bottom_right.longitude = Angle::Degrees(longitude[1]);
  bounds.bottom_right.latitude = Angle::Degrees(latitude[1]);

  return bounds;
  */
}



GeoQuadrilateral
Bitmap::LoadGeoFile([[maybe_unused]] Path path)
{
#ifdef USE_GEOTIFF
  LogFmt("Bitmap::LoadGeoFile: {}", path.c_str());
  // LogFmt("Bitmap::LoadGeoFile: USE_GEOTIFF");
  if (path.EndsWithIgnoreCase(".tif") ||
      path.EndsWithIgnoreCase(".tiff")) {
    auto result = LoadGeoTiff(path);
    if (!LoadFile(path))
      throw std::runtime_error("Failed to use geo image file");

    assert(IsDefined());

    return result.second;
  } else if (path.EndsWithIgnoreCase(".jpg") ||
    path.EndsWithIgnoreCase(".jpeg") ||
    path.EndsWithIgnoreCase(".jfif") ) {
    auto result = LoadFile(path);
    assert(IsDefined());
    if (result) {
      auto name = path.GetBase();
      int offset = 0;
      if (strncmp(name.c_str(), "satellite-", strlen("satellite-")) == 0)
        offset = strlen("satellite-");
      else if (strncmp(name.c_str(), "rain-", strlen("rain-")) == 0)
        offset = strlen("rain-");
 
      return SetTileKoordinates(name.c_str() + offset);
    }
  } else if (path.EndsWithIgnoreCase(".png"))
  {
    auto result = LoadFile(path);
    assert(IsDefined());
    if (result)
      return SetTileKoordinates(path.GetBase().c_str());
  }
  throw std::runtime_error("Unsupported geo image file");
#else  // USE_GEOTIFF
  LogFmt("Bitmap::LoadGeoFile: {}", path.c_str());
#endif  // USE_GEOTIFF
  return {};

}
