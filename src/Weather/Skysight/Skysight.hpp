// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "util/StaticString.hxx"
#include "system/Path.hpp"
#include "LocalPath.hpp"
#include "Operation/VerboseOperationEnvironment.hpp"
#include "time/BrokenDateTime.hpp"
#include "thread/StandbyThread.hpp"
#include "ui/event/Timer.hpp"

#include "Weather/Skysight/Metrics.hpp"
#include "Weather/Skysight/SkysightAPI.hpp"
#include "Blackboard/BlackboardListener.hpp"

#include <map>
#include <vector>
#include <string_view>

#define SKYSIGHT_MAX_METRICS 5

struct BrokenDateTime;
struct DisplayedMetric;

struct SkysightImageFile {
public:
  SkysightImageFile(Path _filename);
  SkysightImageFile(Path _filename, Path _path);
  Path fullpath;
  Path filename;
  std::string metric;
  std::string region;
  uint64_t datetime;
  uint64_t updatetime;
  bool is_valid;
  uint64_t mtime;
};

class Skysight final: private NullBlackboardListener {
public:
  std::string region = "EUROPE";
  DisplayedMetric displayed_metric;
  CurlGlobal *curl;

  Skysight(CurlGlobal &_curl);

  static void APIInited(const std::string details, const bool success,
			const std::string layer_id, const uint64_t time_index);
  static void DownloadComplete(const std::string details, const bool success,
			       const std::string layer_id,
			       const uint64_t time_index);

  std::map<std::string, std::string> GetRegions() {
    return api->regions;
  }

  std::string GetRegion() {
    return api->region;
  }

  SkysightMetric GetMetric(int index) {
    return api->GetMetric(index);
  }

  SkysightMetric *GetMetric(const std::string_view id) {
    return api->GetMetric(id);
  }

  bool MetricExists(const std::string id) {
    return api->MetricExists(id);
  }

  int NumMetrics() {
    return api->NumMetrics();
  }


  void Init();
  bool IsReady(bool force_update = false);

  void SaveActiveMetrics();
  void LoadActiveMetrics();

  void RemoveActiveMetric(int index);
  void RemoveActiveMetric(const std::string id);
  bool ActiveMetricsUpdating();
  bool GetActiveMetricState(std::string metric_name, SkysightActiveMetric &m);
  void SetActveMetricUpdateState(const std::string id, bool state = false);
  void RefreshActiveMetric(std::string id);
  SkysightActiveMetric GetActiveMetric(int index);
  SkysightActiveMetric GetActiveMetric(const std::string id);
  int NumActiveMetrics();
  bool ActiveMetricsFull();
  bool IsActiveMetric(const char *const id);
  int AddActiveMetric(const char *const id);
  bool DownloadActiveMetric(std::string id);
  bool DisplayActiveMetric(const char *const id = nullptr);

  static inline 
  AllocatedPath GetLocalPath() {
    return MakeLocalPath("skysight");
  }

  BrokenDateTime FromUnixTime(uint64_t t);
  BrokenDateTime GetNow(bool use_system_time = false);

  void Render(bool force_update = false);

  static inline Skysight *GetSkysight() { return self;}

  std::string_view GetDisplayedMetricName() { 
    if (Skysight::displayed_metric.metric &&
      !Skysight::displayed_metric.metric->id.empty()) {
      return Skysight::displayed_metric.metric->id;
    } else {
      return "n.a.";
    }
  }

  static DisplayedMetric *GetDisplayedMetric() { 
    if (self->displayed_metric.metric) {
      return &self->displayed_metric;
    } else {
      return nullptr;
    }
  }

protected:
  SkysightAPI *api = nullptr;
  static Skysight *self;

private:
  std::string email;
  std::string password;
  bool update_flag = false;
  BrokenDateTime curr_time;

  /* virtual methods from class BlackboardListener */
  virtual void OnCalculatedUpdate(const MoreData &basic,
                                  const DerivedInfo &calculated) override;

  bool SetDisplayedMetric(const char *const id,
			  BrokenDateTime forecast_time = BrokenDateTime());
  BrokenDateTime GetForecastTime(BrokenDateTime curr_time);
  std::vector<SkysightActiveMetric> active_metrics;

  std::vector<SkysightImageFile> ScanFolder(std::string search_pattern);
  void CleanupFiles();
};
