// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "SkySightRequest.hpp"
#ifdef SKYSIGHT_FORECAST 
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

public:
  std::string region;
  std::map<std::string, SkySight::Region> regions;
  std::map<std::string, SkySight::Layer *> layers;
  std::vector<SkySight::Layer> layers_vector;
  std::vector<SkySight::Layer> selected_layers;

  SkysightAPI() : cache_path(MakeLocalPath("cache/skysight")) {}
  ~SkysightAPI();

  void InitAPI(std::string_view email, std::string_view password,
    std::string_view _region, SkysightCallback cb);
  bool IsInited();
  SkySight::Layer *GetLayer(size_t index);
  SkySight::Layer *GetLayer(const std::string_view id);
  bool LayerExists(const std::string_view id);
  int NumLayers();
  bool SelectedLayersFull();
  bool IsSelectedLayer(const std::string_view id);

  bool GetImageAt(const char *const layer, time_t fctime,
    time_t maxtime, SkysightCallback cb = nullptr);
  bool GetImageAt(SkySight::Layer &layer, time_t fctime,
    time_t maxtime, time_t update_time,
    SkysightCallback cb = nullptr);

  static void GenerateLoginRequest();

  static void MakeCallback(SkysightCallback cb, const std::string &&details,
    const bool success, const std::string &&layer,
    const time_t time_index);
  void TimerInvoke();

  const AllocatedPath  &GetCachePath() { return cache_path; }
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
  inline void ResetLastUpdate() {
    inited_lastupdates = false;
  }
#ifdef SKYSIGHT_FORECAST 
  void CallCDFDecoder(const SkySight::Layer *layer, const time_t _time,
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

  time_t last_request = 0;
  static SkysightAPI *self;
  bool inited_regions = false;
  bool inited_layers = false;
  bool inited_lastupdates = false;
  time_t lastupdates_time = 0;
#ifdef SKYSIGHT_FORECAST 
  SkysightAPIQueue queue;
#endif  // SKYSIGHT_FORECAST 
  const AllocatedPath cache_path;

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

  static void ParseResponse(const std::string &&result, const bool success,
			    const SkysightRequestArgs req);
  bool ParseDataDetails(const SkysightRequestArgs &args,
    const boost::property_tree::ptree &details);

  bool ParseDataDetails(const SkysightRequestArgs &args,
			const std::string &result);

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
