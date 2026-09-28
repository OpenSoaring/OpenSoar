// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "TileLayer.hpp"

#ifdef HAVE_BASE_MAP_TILES

#include "Geo/GeoBounds.hpp"
#include "Geo/Quadrilateral.hpp"
#include "Geo/WebMercator.hpp"
#include "Projection/WindowProjection.hpp"
#include "ui/canvas/Bitmap.hpp"
#include "ui/canvas/custom/GeoBitmap.hpp"
#include "ui/event/CoInjectFunction.hpp"
#include "co/Task.hxx"
#include "io/FileOutputStream.hxx"
#include "io/OutputStream.hxx"
#include "lib/curl/CoStreamRequest.hxx"
#include "lib/curl/Easy.hxx"
#include "lib/curl/Global.hxx"
#include "lib/curl/Setup.hxx"
#include "system/FileUtil.hpp"
#include "LocalPath.hpp"
#include "LogFile.hpp"
#include "Version.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace BaseMap {

const TileSource osm_tile_source{
  "https://tile.openstreetmap.org/{z}/{x}/{y}.png",
  "osm",
  "(c) OpenStreetMap contributors",
  1, 19,
};

/**
 * The OpenStreetMap tile usage policy allows at most two parallel
 * downloads.
 */
static constexpr std::size_t MAX_DOWNLOADS = 2;

/**
 * Decoded tiles kept in memory (as textures): 256x256 RGBA is 256 kB,
 * so this is about 32 MB, enough for two screens at two zoom levels.
 */
static constexpr std::size_t MAX_TILES_IN_MEMORY = 128;

/**
 * Tiles decoded from the cache directory per frame; decoding a PNG
 * takes a few milliseconds on slow devices, so a large map fills in
 * over a few frames instead of stalling one.
 */
static constexpr unsigned LOADS_PER_FRAME = 6;

/**
 * More tiles than this per frame means the scale is far off the tile
 * levels (e.g. an extreme free zoom); drawing is skipped rather than
 * flooding the tile server.
 */
static constexpr unsigned MAX_VISIBLE_TILES = 256;

/**
 * How far up the pyramid a coarser tile is borrowed while the right one
 * is not there yet.
 */
static constexpr unsigned MAX_FALLBACK_LEVELS = 5;

static constexpr std::time_t RETRY_SECONDS = 10 * 60;

static constexpr std::size_t MAX_TILE_BYTES = 2 * 1024 * 1024;

namespace {

class LimitedOutputStream final : public OutputStream {
  OutputStream &destination;
  std::size_t remaining;

public:
  LimitedOutputStream(OutputStream &_destination,
                      std::size_t maximum) noexcept
    :destination(_destination), remaining(maximum) {}

  void Write(std::span<const std::byte> source) override {
    if (source.size() > remaining)
      throw std::runtime_error("Tile is too large");

    destination.Write(source);
    remaining -= source.size();
  }
};

} // anonymous namespace

static std::string
MakeUrl(const char *url_template, unsigned zoom, uint32_t x, uint32_t y)
{
  std::string url{url_template};

  const auto replace = [&url](std::string_view name, unsigned value) {
    const auto i = url.find(name);
    if (i != std::string::npos)
      url.replace(i, name.size(), std::to_string(value));
  };

  replace("{z}", zoom);
  replace("{x}", x);
  replace("{y}", y);
  return url;
}

static Co::Task<bool>
DownloadTile(CurlGlobal &curl, std::string url, AllocatedPath path)
{
  /* FileOutputStream writes to a temporary file which becomes the
     tile only in Commit(), so an aborted download never leaves a
     broken tile in the cache */
  FileOutputStream file(path);
  LimitedOutputStream limited{file, MAX_TILE_BYTES};

  CurlEasy easy{url.c_str()};
  Curl::Setup(easy);
  /* the tile usage policy asks for a User-Agent that identifies the
     application */
  easy.SetUserAgent(XCSoar_ProductToken);
  easy.SetOption(CURLOPT_FOLLOWLOCATION, 1L);
  easy.SetConnectTimeout(20);
  easy.SetTimeout(60);
  easy.SetFailOnError(false);

  const auto response =
    co_await Curl::CoStreamRequest(curl, std::move(easy), limited);
  if (response.status != 200)
    co_return false;

  file.Commit();
  co_return true;
}

struct TileLayer::Download {
  UI::CoInjectFunction<bool> function;

  explicit Download(EventLoop &event_loop) noexcept
    :function(event_loop) {}
};

TileLayer::TileLayer(const TileSource &_source, CurlGlobal &_curl,
                     std::function<void()> _on_tile_arrived)
  :source(_source), curl(_curl),
   cache_path(MakeCacheDirectory(_source.cache_name)),
   on_tile_arrived(std::move(_on_tile_arrived))
{
}

TileLayer::~TileLayer() noexcept
{
  for (auto &i : downloads)
    i.second->function.Cancel();
}

AllocatedPath
TileLayer::GetTilePath(Key key) const noexcept
{
  char name[64];
  snprintf(name, sizeof(name), "%u/%u/%u.png",
           KeyZoom(key), unsigned(KeyX(key)), unsigned(KeyY(key)));
  return AllocatedPath::Build(cache_path, name);
}

MapOverlayBitmap *
TileLayer::Get(Key key, unsigned &load_budget) noexcept
{
  if (auto i = tiles.find(key); i != tiles.end()) {
    i->second.last_used = frame;
    return i->second.bitmap ? &*i->second.bitmap : nullptr;
  }

  if (load_budget == 0 || failed.contains(key))
    return nullptr;

  const auto path = GetTilePath(key);
  if (!File::Exists(path))
    return nullptr;

  --load_budget;

  Bitmap bitmap;
  if (!bitmap.LoadFile(path)) {
    /* a damaged file in our own cache; download it again later */
    LogFormat("Base map: failed to load %s", path.c_str());
    File::Delete(path);
    failed[key] = std::time(nullptr) + RETRY_SECONDS;
    return nullptr;
  }

  const GeoBitmap::TileData tile_data{
    uint16_t(KeyZoom(key)), KeyX(key), KeyY(key),
  };

  auto &tile = tiles[key];
  tile.bitmap.emplace(std::move(bitmap),
                      GeoBitmap::GetGeoQuadrilateral(tile_data), "");
  tile.bitmap->SetWebMercatorTile();
  tile.last_used = frame;
  return &*tile.bitmap;
}

void
TileLayer::Request(Key key) noexcept
{
  if (downloads.contains(key) ||
      std::find(queue.begin(), queue.end(), key) != queue.end())
    return;

  if (auto i = failed.find(key); i != failed.end()) {
    if (std::time(nullptr) < i->second)
      return;
    failed.erase(i);
  }

  queue.push_back(key);
}

void
TileLayer::StartDownloads() noexcept
{
  if (std::time(nullptr) > last_draw + 5)
    queue.clear();

  while (downloads.size() < MAX_DOWNLOADS && !queue.empty()) {
    const Key key = queue.front();
    queue.pop_front();

    const unsigned zoom = KeyZoom(key);
    const uint32_t x = KeyX(key), y = KeyY(key);

    auto path = GetTilePath(key);
    char directory[32];
    snprintf(directory, sizeof(directory), "%u/%u", zoom, unsigned(x));
    Directory::CreateRecursive(AllocatedPath::Build(cache_path, directory));

    auto download = std::make_unique<Download>(curl.GetEventLoop());
    auto &function = download->function;
    downloads.emplace(key, std::move(download));

    function.Start(DownloadTile(curl,
                                MakeUrl(source.url_template, zoom, x, y),
                                std::move(path)),
                   [this, key](bool success) {
                     OnDownloadDone(key, success);
                   },
                   [this, key](std::exception_ptr error) {
                     /* without network, every visible tile fails;
                        one line every few minutes is enough */
                     const auto now = std::time(nullptr);
                     if (now >= last_error_log + 300) {
                       last_error_log = now;
                       LogError(error, "Base map: tile download failed");
                     }
                     OnDownloadDone(key, false);
                   });
  }
}

void
TileLayer::OnDownloadDone(Key key, bool success) noexcept
{
  downloads.erase(key);

  if (success)
    /* forget an earlier "not in the cache" answer */
    tiles.erase(key);
  else
    failed[key] = std::time(nullptr) + RETRY_SECONDS;

  StartDownloads();

  if (success && on_tile_arrived)
    on_tile_arrived();
}

void
TileLayer::Evict() noexcept
{
  if (tiles.size() <= MAX_TILES_IN_MEMORY)
    return;

  std::vector<std::pair<unsigned, Key>> by_age;
  by_age.reserve(tiles.size());
  for (const auto &[key, tile] : tiles)
    if (tile.last_used != frame)
      by_age.emplace_back(tile.last_used, key);

  std::sort(by_age.begin(), by_age.end());

  for (const auto &[last_used, key] : by_age) {
    if (tiles.size() <= MAX_TILES_IN_MEMORY)
      break;
    tiles.erase(key);
  }
}

/**
 * The tile column of a longitude at the given zoom level.
 */
static uint32_t
LongitudeToColumn(Angle longitude, unsigned zoom) noexcept
{
  const uint32_t n = uint32_t{1} << zoom;
  const double x = (longitude.Degrees() + 180.) / 360. * n;
  return uint32_t(std::clamp(std::floor(x), 0., double(n - 1)));
}

/**
 * The tile row of a latitude at the given zoom level; latitudes beyond
 * the Mercator square end up in the first or last row.
 */
static uint32_t
LatitudeToRow(Angle latitude, unsigned zoom) noexcept
{
  const uint32_t n = uint32_t{1} << zoom;
  const double y = (1. - WebMercator::LatitudeToY(latitude) / M_PI) / 2. * n;
  return uint32_t(std::clamp(std::floor(y), 0., double(n - 1)));
}

void
TileLayer::Draw(Canvas &canvas, const WindowProjection &projection) noexcept
{
  ++frame;
  last_draw = std::time(nullptr);

  const GeoBounds &bounds = projection.GetScreenBounds();
  if (!bounds.IsValid())
    return;

  const unsigned zoom =
    unsigned(std::clamp<long>(std::lround(projection.GetZoomLevel()),
                              source.min_zoom, source.max_zoom));

  const uint32_t x_west = LongitudeToColumn(bounds.GetWest(), zoom);
  const uint32_t x_east = LongitudeToColumn(bounds.GetEast(), zoom);
  const uint32_t y_north = LatitudeToRow(bounds.GetNorth(), zoom);
  const uint32_t y_south = LatitudeToRow(bounds.GetSouth(), zoom);

  /* across the antimeridian, the columns wrap around */
  const uint32_t n = uint32_t{1} << zoom;
  const uint32_t columns = x_east >= x_west
    ? x_east - x_west + 1
    : n - x_west + x_east + 1;
  const uint32_t rows = y_south - y_north + 1;
  if (columns * rows > MAX_VISIBLE_TILES)
    return;

  /* the tiles closest to the screen center are requested first, so
     the map fills in from where the pilot is looking */
  std::vector<std::pair<double, Key>> wanted;
  wanted.reserve(columns * rows);
  for (uint32_t r = 0; r < rows; ++r) {
    for (uint32_t c = 0; c < columns; ++c) {
      const double distance = std::hypot(c + 0.5 - columns / 2.,
                                         r + 0.5 - rows / 2.);
      wanted.emplace_back(distance,
                          MakeKey(zoom, (x_west + c) % n, y_north + r));
    }
  }
  std::sort(wanted.begin(), wanted.end());

  /* only the tiles of this frame are worth downloading; a request that
     scrolled out of view is dropped before it starts */
  queue.clear();

  unsigned load_budget = LOADS_PER_FRAME;
  std::vector<MapOverlayBitmap *> visible;
  std::map<Key, MapOverlayBitmap *> fallbacks;

  for (const auto &[distance, key] : wanted) {
    if (auto *bitmap = Get(key, load_budget)) {
      visible.push_back(bitmap);
      continue;
    }

    Request(key);

    /* until it arrives, a coarser tile from the cache stands in */
    const uint32_t x = KeyX(key), y = KeyY(key);
    for (unsigned d = 1; d <= MAX_FALLBACK_LEVELS && d <= zoom - source.min_zoom; ++d) {
      const Key parent = MakeKey(zoom - d, x >> d, y >> d);
      if (fallbacks.contains(parent))
        break;

      if (auto *bitmap = Get(parent, load_budget)) {
        fallbacks.emplace(parent, bitmap);
        break;
      }
    }
  }

  StartDownloads();

  /* coarse stand-ins first (lowest zoom first), the real tiles on top */
  for (const auto &[key, bitmap] : fallbacks)
    bitmap->Draw(canvas, projection);

  for (auto *bitmap : visible)
    bitmap->Draw(canvas, projection);

  Evict();
}

} // namespace BaseMap

#endif
