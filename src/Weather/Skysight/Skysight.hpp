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

#include "Weather/Skysight/Layers.hpp"
#include "Weather/Skysight/SkysightAPI.hpp"
#include "Blackboard/BlackboardListener.hpp"

#include <map>
#include <vector>
#include <string_view>

#define SKYSIGHT_MAX_METRICS 5

struct BrokenDateTime;
struct DisplayedLayer;
class CurlGlobal;

struct SkysightImageFile {
public:
  SkysightImageFile(Path _filename);
  SkysightImageFile(Path _filename, Path _path);
  Path fullpath;
  Path filename;
  std::string layer;
  std::string region;
  uint64_t datetime;
  uint64_t updatetime;
  bool is_valid;
  uint64_t mtime;
};

class Skysight final: private NullBlackboardListener {
public:
  std::string region = "EUROPE";
  DisplayedLayer displayed_layer;
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

  SkysightLayer GetLayer(int index) {
    return api->GetLayer(index);
  }

  SkysightLayer *GetLayer(const std::string_view id) {
    return api->GetLayer(id);
  }

  bool LayerExists(const std::string id) {
    return api->LayerExists(id);
  }

  int NumLayers() {
    return api->NumLayers();
  }


  void Init();
  bool IsReady(bool force_update = false);

  void SaveActiveLayers();
  void LoadActiveLayers();

  void RemoveActiveLayer(int index);
  void RemoveActiveLayer(const std::string id);
  bool ActiveLayersUpdating();
  bool GetActiveLayerState(std::string layer_name, SkysightActiveLayer &m);
  void SetActveLayerUpdateState(const std::string id, bool state = false);
  void RefreshActiveLayer(std::string id);
  SkysightActiveLayer GetActiveLayer(int index);
  SkysightActiveLayer GetActiveLayer(const std::string id);
  int NumActiveLayers();
  bool ActiveLayersFull();
  bool IsActiveLayer(const char *const id);
  int AddActiveLayer(const char *const id);
  bool DownloadActiveLayer(std::string id);
  bool DisplayActiveLayer(const char *const id = nullptr);

  static inline 
  AllocatedPath GetLocalPath() {
    return MakeLocalPath("skysight");
  }

  BrokenDateTime FromUnixTime(uint64_t t);
  BrokenDateTime GetNow(bool use_system_time = false);

  void Render(bool force_update = false);

  static inline Skysight *GetSkysight() { return self;}

  std::string_view GetDisplayedLayerName() { 
    if (Skysight::displayed_layer.layer &&
      !Skysight::displayed_layer.layer->id.empty()) {
      return Skysight::displayed_layer.layer->id;
    } else {
      return "n.a.";
    }
  }

  static DisplayedLayer *GetDisplayedLayer() { 
    if (self->displayed_layer.layer) {
      return &self->displayed_layer;
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

  bool SetDisplayedLayer(const char *const id,
			  BrokenDateTime forecast_time = BrokenDateTime());
  BrokenDateTime GetForecastTime(BrokenDateTime curr_time);
  std::vector<SkysightActiveLayer> active_layers;

  std::vector<SkysightImageFile> ScanFolder(std::string search_pattern);
  void CleanupFiles();
};
