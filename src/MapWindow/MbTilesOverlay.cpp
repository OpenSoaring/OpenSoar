// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "MbTilesOverlay.hpp"

#include "Geo/GeoPoint.hpp"
#include "Math/Angle.hpp"
#include "Projection/WindowProjection.hpp"
#include "ui/canvas/Canvas.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

MbTilesOverlay::MbTilesOverlay(Path path, std::string _label)
  :database(path), label(std::move(_label))
{
  cached_zoom = database.GetMetadata().max_zoom;
}

unsigned
MbTilesOverlay::SelectZoom(const WindowProjection &projection) const noexcept
{
  /* the map is drawn in Web Mercator, so the tile level that matches
     the screen pixels is the map's own zoom level; when the map is
     locked to a level, the tiles are shown pixel for pixel */
  const int zoom = int(std::lround(projection.GetZoomLevel()));
  return unsigned(std::clamp<int>(zoom,
                                  database.GetMetadata().min_zoom,
                                  database.GetMetadata().max_zoom));
}

MapOverlayBitmap
MbTilesOverlay::LoadTile(TileKey key)
{
  Bitmap bitmap = database.LoadTile(key);

  GeoQuadrilateral bounds{
    key.GetNorthWest(),
    key.GetNorthEast(),
    key.GetSouthWest(),
    key.GetSouthEast(),
  };

  MapOverlayBitmap overlay(std::move(bitmap), bounds, "");
  overlay.SetAlpha(1.0);
  overlay.SetWebMercatorTile();
  return overlay;
}

bool
MbTilesOverlay::IsInside(GeoPoint p) const noexcept
{
  const auto &bounds = database.GetMetadata().bounds;
  return !bounds.IsValid() || bounds.IsInside(p);
}

void
MbTilesOverlay::Draw(Canvas &canvas, const WindowProjection &projection) noexcept
{
  const auto &screen_bounds = projection.GetScreenBounds();
  if (database.GetMetadata().bounds.IsValid() &&
      !database.GetMetadata().bounds.Overlaps(screen_bounds))
    return;

  const unsigned zoom = SelectZoom(projection);
  if (zoom != cached_zoom) {
    cache.clear();
    cached_zoom = zoom;
  }

  const unsigned scale = 1u << zoom;
  const TileKey south_west_key = TileKey::FromGeoPoint(screen_bounds.GetSouthWest(), zoom);
  const TileKey north_east_key = TileKey::FromGeoPoint(screen_bounds.GetNorthEast(), zoom);

  const unsigned int min_column = south_west_key.column;
  const unsigned int max_column = std::min(scale - 1, north_east_key.column);
  const unsigned int min_row = south_west_key.row;
  const unsigned int max_row = std::min(scale - 1, north_east_key.row);

  for (unsigned int row = min_row; row <= max_row; ++row) {
    for (unsigned int column = min_column; column <= max_column; ++column) {
      const TileKey key{zoom, column, row};
      auto it = cache.find(key);
      if (it == cache.end()) {
        try {
          if (!database.HasTile(key)) {
            it = cache.emplace(key, std::nullopt).first;
          } else {
            try {
              it = cache.emplace(key, LoadTile(key)).first;
            } catch (const std::exception &) {
              it = cache.emplace(key, std::nullopt).first;
            }
          }
        } catch (const std::exception &) {
          it = cache.emplace(key, std::nullopt).first;
        }
      }

      if (!it->second)
        continue;

      it->second->Draw(canvas, projection);
    }
  }
}
