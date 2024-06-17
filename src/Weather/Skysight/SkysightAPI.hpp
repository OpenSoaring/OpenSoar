// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once


#include "Request.hpp"
#include "APIGlue.hpp"
#include "APIQueue.hpp"
#include "Metrics.hpp"
#include <memory>
#include <map>
#include "system/Path.hpp"
#include "LocalPath.hpp"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

//maintain two-hour local data cache
#define SKYSIGHTAPI_LOCAL_CACHE 7200 

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
  std::vector<SkysightMetric> metrics;

  SkysightAPI(std::string email, std::string password, std::string _region,
	      SkysightCallback cb);
  ~SkysightAPI();
  
  bool IsInited();
  SkysightMetric GetMetric(int index);
  SkysightMetric GetMetric(const std::string id);
  SkysightMetric *GetMetric(const char *const id);
  bool MetricExists(const std::string id);
  int NumMetrics();

  bool GetImageAt(const char *const layer, BrokenDateTime fctime,
		  BrokenDateTime maxtime, SkysightCallback cb = nullptr);

  BrokenDateTime FromUnixTime(uint64_t t);
  static void GenerateLoginRequest();

  static void MakeCallback(SkysightCallback cb, const std::string &&details,
        const bool success, const std::string &&layer,
        const uint64_t time_index);

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
  GetUrl(SkysightCallType type, const char *const layer = nullptr,
	 const uint64_t from = 0); 
  inline AllocatedPath
  GetPath(SkysightCallType type, const char *const layer = nullptr,
	  const uint64_t fctime = 0);

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
    return GetData(t,  nullptr,  0, 0, nullptr, cb, force_recache);
  }

  inline bool
  GetData(SkysightCallType t, const char *const layer, uint64_t from,
	  uint64_t to,
	  SkysightCallback cb = nullptr,  bool force_recache = false) {
    return GetData(t, layer,  from, to, nullptr, cb, force_recache);
  }

  bool
  GetData(SkysightCallType t, const char *const layer, const uint64_t from,
	  const uint64_t to, const char *const link,
	  SkysightCallback cb = nullptr, bool force_recache = false);

  bool Login(const SkysightCallback cb = nullptr);

};
