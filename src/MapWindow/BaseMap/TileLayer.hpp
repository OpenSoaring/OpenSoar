// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Features.hpp"

#ifdef HAVE_BASE_MAP_TILES

#include "MapWindow/Overlay.hpp"
#include "MapWindow/OverlayBitmap.hpp"
#include "system/Path.hpp"

#include <cstdint>
#include <ctime>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

class CurlGlobal;

namespace UI { template<typename T> class CoInjectFunction; }

namespace BaseMap {

/**
 * Describes a slippy map tile server.
 */
struct TileSource {
  /**
   * The URL with the placeholders {z}, {x} and {y}.
   */
  const char *url_template;

  /**
   * The name of the directory below the cache directory.
   */
  const char *cache_name;

  /**
   * The copyright notice that must be visible while the tiles are
   * shown.
   */
  const char *attribution;

  unsigned min_zoom, max_zoom;
};

/**
 * The OpenStreetMap standard tile layer.  Its tile usage policy asks
 * for a valid User-Agent, no more than two parallel connections, local
 * caching and a visible attribution; see
 * https://operations.osmfoundation.org/policies/tiles/
 */
extern const TileSource osm_tile_source;

/**
 * Draws the tiles of a #TileSource that cover the map, downloading
 * missing ones in the background and keeping them in a cache directory
 * so that the map also works offline where it was shown before.
 *
 * The map is drawn in Web Mercator at the tile zoom levels, so the
 * tiles are placed pixel for pixel; see Projection::SetZoomLevel().
 *
 * All methods must be called in the UI thread.
 */
class TileLayer final : public MapOverlay {
  using Key = uint64_t;

  struct Tile {
    std::optional<MapOverlayBitmap> bitmap;

    /**
     * The frame in which this tile was drawn last; old tiles are
     * dropped first when the cache is full.
     */
    unsigned last_used = 0;
  };

  struct Download;

  const TileSource &source;
  CurlGlobal &curl;
  const AllocatedPath cache_path;

  /**
   * Called when a download has finished, so the map can be redrawn.
   */
  const std::function<void()> on_tile_arrived;

  std::map<Key, Tile> tiles;

  /**
   * Tiles that are not in the cache directory yet, in the order in
   * which they were requested.
   */
  std::deque<Key> queue;

  std::map<Key, std::unique_ptr<Download>> downloads;

  /**
   * Tiles that failed to download or to load, with the time at which
   * the next attempt is allowed.
   */
  std::map<Key, std::time_t> failed;

  unsigned frame = 0;

  /**
   * When Draw() ran last.  Queued downloads only continue while the
   * layer is on the screen, so nothing is fetched for a page that was
   * left.
   */
  std::time_t last_draw = 0;

  std::time_t last_error_log = 0;

public:
  TileLayer(const TileSource &_source, CurlGlobal &_curl,
            std::function<void()> _on_tile_arrived);
  ~TileLayer() noexcept override;

  const TileSource &GetSource() const noexcept {
    return source;
  }

  /* virtual methods from class MapOverlay */
  const char *GetLabel() const noexcept override {
    return source.attribution;
  }

  bool IsInside(GeoPoint) const noexcept override {
    return true;
  }

  void Draw(Canvas &canvas,
            const WindowProjection &projection) noexcept override;

private:
  static constexpr Key MakeKey(unsigned zoom, uint32_t x,
                               uint32_t y) noexcept {
    return (Key(zoom) << 58) | (Key(x) << 29) | Key(y);
  }

  static constexpr unsigned KeyZoom(Key key) noexcept {
    return unsigned(key >> 58);
  }

  static constexpr uint32_t KeyX(Key key) noexcept {
    return uint32_t(key >> 29) & ((1u << 29) - 1);
  }

  static constexpr uint32_t KeyY(Key key) noexcept {
    return uint32_t(key) & ((1u << 29) - 1);
  }

  AllocatedPath GetTilePath(Key key) const noexcept;

  /**
   * Returns the tile if it is in memory, loading it from the cache
   * directory if possible (at most @p load_budget loads per frame).
   */
  MapOverlayBitmap *Get(Key key, unsigned &load_budget) noexcept;

  void Request(Key key) noexcept;
  void StartDownloads() noexcept;
  void OnDownloadDone(Key key, bool success) noexcept;
  void Evict() noexcept;
};

} // namespace BaseMap

#endif
