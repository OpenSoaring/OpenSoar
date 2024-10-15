// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Request.hpp"
#include "APIGlue.hpp"
#include "APIQueue.hpp"
#include "Layers.hpp"
#include "system/Path.hpp"
#include "LocalPath.hpp"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <memory>
#include <map>

#ifdef __MSVC__
// #define USE_STD_FORMAT
#endif
#define SKYSIGHT_DEBUG 1

//maintain two-hour local data cache
#define SKYSIGHTAPI_LOCAL_CACHE 7200 
#define SKYSIGHT_MAX_LAYERS 5


#define SKYSIGHTAPI_BASE_URL "https://skysight.io/api"

struct BrokenDateTime;

class SkysightAPI final {
  friend struct SkysightRequest;
  friend struct SkysightAsyncRequest;
  friend class CDFDecoder;
  UI::PeriodicTimer timer{[this]{ OnTimer(); }};
  
public:
  std::string region;
  std::map<std::string, std::string> regions;
  std::map<std::string, const SkysightLayer *> layers;
  std::vector<SkysightLayer> layers_vector;
  std::vector<SkysightLayer> selected_layers;

  SkysightAPI(std::string email, std::string password, std::string _region,
	      SkysightCallback cb);
  ~SkysightAPI();
  
  bool IsInited();
  SkysightLayer *GetLayer(size_t index);
  SkysightLayer *GetLayer(const std::string_view id);
  bool LayerExists(const std::string_view id);
  int NumLayers();
  bool SelectedLayersFull();
  bool IsSelectedLayer(const std::string_view id);

  bool GetImageAt(const char *const layer, BrokenDateTime fctime,
		  BrokenDateTime maxtime, SkysightCallback cb = nullptr);
  bool GetImageAt(/* const */ SkysightLayer &layer, BrokenDateTime fctime,
                  BrokenDateTime maxtime, uint64_t update_time,
                  SkysightCallback cb = nullptr);

  static void GenerateLoginRequest();

  static void MakeCallback(SkysightCallback cb, const std::string &&details,
        const bool success, const std::string &&layer,
        const time_t time_index);
  void TimerInvoke();

  AllocatedPath
  GetPath(SkysightCallType type, const std::string_view layer_id = "",
      const time_t fctime = 0);

protected:
  static SkysightAPI *self;
  bool inited_regions;
  bool inited_layers;
  bool inited_lastupdates;
  SkysightAPIQueue queue;
  const AllocatedPath cache_path;

  void LoadDefaultRegions();
  
  bool IsLoggedIn();
  void OnTimer();
  inline const std::string
  GetUrl(SkysightCallType type, const std::string_view layer_id = "",
	  const time_t from = 0); 

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
			const std::string &result);
  bool ParseData(const SkysightRequestArgs &args, const std::string &result);
  bool ParseLogin(const SkysightRequestArgs &args, const std::string &result);

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

  bool Login(const SkysightCallback cb = nullptr);

};
