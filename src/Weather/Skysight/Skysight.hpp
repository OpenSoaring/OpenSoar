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
  SkysightLayer *active_layer = nullptr;
public:
  std::string region = "EUROPE";
  CurlGlobal *curl;

  static SkysightLayer *GetActiveLayer() { return self->active_layer; }

  Skysight(CurlGlobal &_curl);

  static void APIInited(const std::string details, const bool success,
      const std::string layer_id, const uint64_t time_index);
#if 1  // used in API (in ParseLastUpdates())
  static void DownloadComplete(const std::string details, const bool success,
      const std::string layer_id, const uint64_t time_index);
#endif

  std::map<std::string, std::string> GetRegions() {
    return api->regions;
  }

  std::string GetRegion() {
    return api->region;
  }

  SkysightLayer *GetLayer(size_t index) {
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

  void SaveSelectedLayers();
  void LoadSelectedLayers();

  void RemoveSelectedLayer(size_t index);
  void RemoveSelectedLayer(const std::string_view id);
  bool SelectedLayersUpdating();
  bool GetSelectedLayerState(const std::string_view layer_name, SkysightLayer &m);
#if 1  // used in API (in ParseLastUpdates())
  void SetSelectedLayerUpdateState(const std::string_view id, bool state = false);
#endif
  void RefreshSelectedLayer(const std::string_view id);
  SkysightLayer *GetSelectedLayer(int index);
  SkysightLayer *GetSelectedLayer(const std::string_view id);
  size_t NumSelectedLayers();
  bool SelectedLayersFull();
  size_t AddSelectedLayer(const std::string_view id);
#if 0  // def _DEBUG
  bool DownloadSelectedLayer(const std::string_view id);
#endif

//  bool IsSelectedLayer(const std::string_view id);
  bool LayerExists(const std::string_view id);
  // bool DisplaySelectedLayer(const std::string_view id = "");
  bool DisplayActiveLayer();
  
  void DeactivateLayer();
  bool SetLayerActive(const std::string_view id);

  static inline 
  AllocatedPath GetLocalPath() {
    return MakeLocalPath("skysight");
  }

  BrokenDateTime FromUnixTime(uint64_t t);
  BrokenDateTime GetNow(bool use_system_time = false);

  void Render(bool force_update = false);

  static inline Skysight *GetSkysight() { return self;}

  std::string_view GetActiveLayerName() { 
    if (active_layer) {
      return active_layer->name;
    } else {
      return "n.a.";
    }
  }

  std::string_view GetActiveLayerId() { 
    if (active_layer) {
      return active_layer->id;
    } else {
      return "";
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

  bool SetActiveLayer(const std::string_view id,
        BrokenDateTime forecast_time = BrokenDateTime());
  BrokenDateTime GetForecastTime(BrokenDateTime curr_time);

  std::vector<SkysightImageFile> ScanFolder(std::string search_pattern);
  void CleanupFiles();
};
