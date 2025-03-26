#pragma once

#include <stdint.h>
class GeoBounds;
struct GeoQuadrilateral;
class MapWindowProjection;

namespace GeoBitmap {
  struct TileData {
    uint16_t zoom;
    uint16_t x;
    uint16_t y;

    bool valid()
    { return (zoom > 0) && (x > 0) && (y > 0); }
  };

  TileData GetTile(const GeoBounds &bounds, const uint16_t zoom);
  TileData GetTile(const MapWindowProjection &proj,
    const uint16_t zoom_min = 1, const uint16_t zoom_max = 20);
  GeoBounds GetBounds(const TileData &data);
  GeoQuadrilateral GetGeoQuadrilateral(const TileData &data);

};
