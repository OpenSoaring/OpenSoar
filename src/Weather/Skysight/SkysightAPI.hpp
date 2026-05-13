// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "SkySightRequest.hpp"
#ifdef SKYSIGHT_FORECAST
# define SKYSIGHT_TIME_DELAY 5
// TODO(August2111) for this there is to be cleanup all SkysightCallback
# include "APIQueue.hpp"
#else
# include "APIGlue.hpp"
#endif  // SKYSIGHT_FORECAST 
#include "ui/event/PeriodicTimer.hpp"

#include "Layers.hpp"
#include "system/Path.hpp"
#include "LocalPath.hpp"
#include "ui/canvas/custom/GeoBitmap.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <memory>
#include <map>

constexpr time_t  ONE_MINUTE = 60;
constexpr time_t  HALF_MINUTE = 30;
constexpr time_t  HALF_HOUR = (30 * ONE_MINUTE);
constexpr time_t  ONE_HOUR = (60 * ONE_MINUTE);
constexpr time_t  TEN_MINUTES = (10 * ONE_MINUTE);
constexpr time_t  ONE_DAY = (24 * ONE_HOUR);
constexpr time_t  HALF_DAY = (12 * ONE_HOUR);

constexpr uint16_t  TILE_RANGE_OFFSET = 2;  //  -> 25 tiles!

#ifdef _DEBUG
constexpr time_t  ONLINE_FORECAST = (ONE_HOUR);
constexpr time_t  OFFLINE_FORECAST = (6 * ONE_HOUR);
/* diff time before switching to new forcast*/
#else
constexpr time_t  ONLINE_FORECAST = (2 * ONE_HOUR);
constexpr time_t  OFFLINE_FORECAST = (24 * ONE_HOUR);
#endif
constexpr time_t  FORECAST_OFFSET = TEN_MINUTES;

// #define SKYSIGHT_DEBUG 1

// maintain two-hour local data cache
// #define SKYSIGHTAPI_LOCAL_CACHE (2 * ONE_HOUR) 
#define SKYSIGHT_MAX_LAYERS 8

namespace SkySight{
  struct Region {
    std::string id;
    std::string name;
    std::string projection;
    std::string update;
    time_t update_interval;
    time_t update_times[6];
    // bounds
    std::string timezone;
  };
};

const std::string SKYSIGHTAPI_BASE_URL("https://skysight.io/api");
const std::string OSM_BASE_URL("https://tile.openstreetmap.org");

class SkysightAPI final {
  friend class CDFDecoder;
  UI::PeriodicTimer timer{ [this] { OnTimer(); } };
  SkysightRequest *co_request = nullptr;
  /** Cache directory for Skysight forecast data (= <cache_root>/skysight). */
  static AllocatedPath cache_path;
  /** Cache directory for OSM tiles (= <cache_root>/osm). */
  static AllocatedPath osm_path;

public:
  std::string region;
  std::map<std::string, SkySight::Region> regions;
  std::map<std::string, SkySight::Layer *> layers;
  std::vector<SkySight::Layer> layers_vector;
  std::vector<SkySight::Layer *> selected_layers;

  SkysightAPI() {
    // Ensure both cache directories exist exactly once.
    // cache_path   = <cache_root>/skysight  (Skysight forecast cache)
    // osm_path     = <cache_root>/osm       (OSM tile cache)
    if (cache_path == nullptr)
      cache_path = MakeCacheDirectory("skysight");
    if (osm_path == nullptr)
      osm_path = MakeCacheDirectory("osm");
  }
  ~SkysightAPI();

  void InitAPI(std::string_view email, std::string_view password,
    std::string_view _region, SkysightCallback cb);
  bool IsInited();
  SkySight::Layer *GetLayer(size_t index);
  SkySight::Layer *GetLayer(const std::string_view id);
  bool LayerExists(const std::string_view id);
  size_t AddSelectedLayer(const std::string_view id);
  size_t NumLayers();
  bool SelectedLayersFull();
  bool IsSelectedLayer(const std::string_view id);

  static void GenerateLoginRequest();

  static void MakeCallback(SkysightCallback cb, const std::string &&details,
    const bool success, const std::string &&layer,
    const time_t time_index);
  void TimerInvoke();

  /**
   * Returns the Skysight forecast cache directory (= <cache_root>/skysight)
   * and lazily creates it if it has not been initialised yet.
   * Single source of truth, used by Skysight::GetLocalPath().
   */
  static const AllocatedPath &GetCachePath() {
    if (cache_path == nullptr)
      cache_path = MakeCacheDirectory("skysight");
    return cache_path;
  }
  AllocatedPath
    GetPath(SkysightCallType type, const std::string_view layer_id = "",
      const time_t fctime = 0, const GeoBitmap::TileData tile = { 0 });
#ifdef SKYSIGHT_FORECAST 
  bool QueueIsEmpty() {
    return queue.IsEmpty();
   }
  bool QueueIsLastJob() {
    return queue.IsLastJob();
  }
#endif
#ifdef SKYSIGHT_FORECAST 
  bool BuildForecastTiff(const Path filename);
  void CallCDFDecoder(const SkySight::Layer *layer,
    const std::string_view &cdf_file, const std::string_view &output_img,
    const SkysightCallback _callback);
#endif  // SKYSIGHT_FORECAST 

  // access fro SkySightRequest:
  bool UpdateRegions(const boost::json::value &details);
  bool UpdateLayers(const boost::json::value &details);
  bool UpdateLastUpdates(const boost::json::value &details);
  bool UpdateDatafiles(const boost::json::value &details);

protected:
  /// The mutex protects the Timer module.
  Mutex mutex;

#ifdef SKYSIGHT_TIME_DELAY
  time_t last_request = 0;
#endif
  static SkysightAPI *self;
  bool inited_regions = false;
  bool inited_layers = false;
  bool inited_lastupdates = false;
  time_t lastupdates_time = 0;
#ifdef SKYSIGHT_FORECAST 
  SkysightAPIQueue queue;
#endif  // SKYSIGHT_FORECAST 

  void LoadDefaultRegions(const std::string_view select_region);

  bool IsLoggedIn();
  void OnTimer();
  inline const std::string
    GetUrl(SkysightCallType type, const std::string_view layer_id = "",
      const time_t from = 0, const GeoBitmap::TileData tile = {0});

  bool GetResult(const SkysightRequestArgs &args, const std::string result,
		 boost::property_tree::ptree &output);
  bool CacheAvailable(Path path, SkysightCallType calltype,
		      const char *const layer = nullptr);

  inline bool GetData(SkysightCallType t, SkysightCallback cb = nullptr,
		      bool force_recache = false) {
    return GetData(t, "", 0, 0, "", cb, force_recache);
  }

  inline bool
  GetData(SkysightCallType t, const std::string_view layer_id, time_t from,
	  time_t to,
	  SkysightCallback cb = nullptr,  bool force_recache = false) {
    return GetData(t, layer_id, from, to, "", cb, force_recache);
  }

  bool GetData(SkysightCallType t, const std::string_view layer_id,
    const time_t from, const time_t to, const std::string_view link,
	  SkysightCallback cb = nullptr, bool force_recache = false);

  bool GetTileData(const std::string_view layer_id, const time_t from,
	  const time_t to, SkysightCallback cb = nullptr, 
    bool force_recache = false);

  bool Login(const SkysightCallback cb = nullptr);
};
