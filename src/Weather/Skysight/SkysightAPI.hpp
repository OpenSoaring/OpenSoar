// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Request.hpp"
#include "APIGlue.hpp"
#include "APIQueue.hpp"
#include "Layers.hpp"
#include "system/Path.hpp"
#include "LocalPath.hpp"
#include "ui/canvas/custom/GeoBitmap.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <memory>
#include <map>

constexpr time_t  _HALFHOUR = (60 * 30);
constexpr time_t  _ONE_HOUR = (60 * 60);
constexpr time_t  _10MINUTES = (60 * 10);

// #define SKYSIGHT_DEBUG 1

//maintain two-hour local data cache
#define SKYSIGHTAPI_LOCAL_CACHE 7200 
#define SKYSIGHT_MAX_LAYERS 8


constexpr char const *SKYSIGHTAPI_BASE_URL = "https://skysight.io/api";
constexpr char const *OSM_BASE_URL = "https://tile.openstreetmap.org";

struct BrokenDateTime;

class SkysightAPI final {
  friend struct SkysightRequest;
  friend struct SkysightAsyncRequest;
  friend class CDFDecoder;
  UI::PeriodicTimer timer{ [this] { OnTimer(); } };

public:
  std::string region;
  std::map<std::string, std::string> regions;
  std::map<std::string, const SkysightLayer *> layers;
  std::vector<SkysightLayer> layers_vector;
  std::vector<SkysightLayer> selected_layers;

  SkysightAPI(std::string_view email, std::string_view password,
              std::string_view _region,
    SkysightCallback cb);
  ~SkysightAPI();

  bool IsInited();
  SkysightLayer *GetLayer(size_t index);
  SkysightLayer *GetLayer(const std::string_view id);
  bool LayerExists(const std::string_view id);
  int NumLayers();
  bool SelectedLayersFull();
  bool IsSelectedLayer(const std::string_view id);

  bool GetImageAt(const char *const layer, time_t fctime,
    time_t maxtime, SkysightCallback cb = nullptr);
  bool GetImageAt(/* const */ SkysightLayer &layer, time_t fctime,
    time_t maxtime, time_t update_time,
    SkysightCallback cb = nullptr);

  static void GenerateLoginRequest();

  static void MakeCallback(SkysightCallback cb, const std::string &&details,
    const bool success, const std::string &&layer,
    const time_t time_index);
  void TimerInvoke();

  AllocatedPath
    GetPath(SkysightCallType type, const std::string_view layer_id = "",
      const time_t fctime = 0, const GeoBitmap::TileData tile = { 0 });
  bool QueueIsEmpty() {
    return queue.IsEmpty();
   }
  bool QueueIsLastJob() {
    return queue.IsLastJob();
  }
protected:
  static SkysightAPI *self;
  bool inited_regions = false;
  bool inited_layers = false;
  bool inited_lastupdates = false;
  time_t lastupdates_time = 0;
  SkysightAPIQueue queue;
  const AllocatedPath cache_path;

  void LoadDefaultRegions();

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
  bool ParseRegions(const SkysightRequestArgs &args, const std::string &result);
  bool ParseLayers(const SkysightRequestArgs &args, const std::string &result);
  bool ParseLastUpdates(const SkysightRequestArgs &args,
			const std::string &result);
  bool ParseDataDetails(const SkysightRequestArgs &args,
    const boost::property_tree::ptree &details);

  bool ParseDataDetails(const SkysightRequestArgs &args,
			const std::string &result);
  bool ParseData(const SkysightRequestArgs &args, const std::string &result);
  bool ParseTile(const SkysightRequestArgs &args, const std::string &result);
  bool ParseLogin(const SkysightRequestArgs &args, const std::string &result);
  void CallCDFDecoder(const SkysightRequestArgs &args,
      const std::string_view &output_img);

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

  bool
  GetData(SkysightCallType t, const std::string_view layer_id, const time_t from,
	  const time_t to, const std::string_view link,
	  SkysightCallback cb = nullptr, bool force_recache = false);

  bool
  GetTileData(const std::string_view layer_id, const time_t from,
	  const time_t to, SkysightCallback cb = nullptr, 
    bool force_recache = false);

  bool Login(const SkysightCallback cb = nullptr);

};
