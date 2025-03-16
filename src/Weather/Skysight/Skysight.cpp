// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Skysight.hpp"
#include "system/Path.hpp"
#include "LocalPath.hpp"
#include "system/FileUtil.hpp"
#include "util/StringCompare.hxx"
#include "util/Macros.hpp"
#include "util/StaticString.hxx"
#include "Profile/Profile.hpp"
#include "ActionInterface.hpp"
#include "system/FileUtil.hpp"
#include "Interface.hpp"
#include "UIGlobals.hpp"
#include "Language/Language.hpp"
#include "LogFile.hpp"
#include "time/BrokenDateTime.hpp"
#include "MapWindow/OverlayBitmap.hpp"
#include "MapWindow/GlueMapWindow.hpp"
#include "thread/Debug.hpp"
#include "time/DateTime.hpp"
#include "Formatter/TimeFormatter.hpp"

#include "MainWindow.hpp"

#include <string>
#include <vector>
#include <memory>
#include <thread>

static uint16_t overlay_index = 0;
/**
 * TODO:
 * -- overlay only shows following render -- no way to trigger from child 
 *    thread
 * -- no transparent bg on overlay on android
 *
 * --- for release ----
 * - Use SkysightImageFile elsewhere instead of recalculating forecast time,
 *   move to separate imp file
 * - clean up libs
 * - rebase on latest master, clean up
 * - move cache trimming to API?
 * - clean up layers/activelayers/displayed_layer -- inheritance rather
 *   than pointer
 * - Add documentation
 * - Test cubie compile / libs

 --- style ----
 * fix variable style/case,
 * reduce use of STL strings
 * Can use AtScopeExit for object cleanup in tiff generation
 * Use consistent string conventions ( _()? )
 * replace #defines in self.hpp with better c++ idioms
* Use static_cast<> instead of c casts
 */

Skysight *Skysight::self = nullptr;

/*
 *
 * Img File
 *
 */
SkysightImageFile::SkysightImageFile(Path _filename) {
  filename = _filename;
  fullpath = AllocatedPath::Build(Skysight::GetLocalPath(), filename);
  SkysightImageFile(filename, fullpath);
}

SkysightImageFile::SkysightImageFile(Path _filename, Path _path) { 
  filename = _filename;
  fullpath = _path;
  region = std::string(_("INVALID"));
  layer = std::string(_("INVALID"));
  datetime = 0;
  is_valid = false;
  mtime = 0;

  // images are in format region-layer-datetime.tif
  if (!filename.EndsWithIgnoreCase(".tif"))
    return;

  std::string file_base = filename.GetBase().c_str();

  std::size_t p = file_base.find(_("-"));
  if (p == std::string::npos)
    return;

  std::string reg = file_base.substr(0, p);
  std::string rem = file_base.substr(p+1);

  p = rem.find(_("-"));
  if (p == std::string::npos)
    return;
  std::string met = rem.substr(0, p);
  datetime = std::stoi(rem.substr(p + 1));
  mtime = File::GetTime(fullpath);

  region = reg;
  layer = met;
  is_valid = true;
}

/*
 * ******   SELECTED LAYERS ************
 *
 */
bool
Skysight::SelectedLayersFull()
{
  assert(api);
  return (api->SelectedLayersFull());
}

bool
Skysight::LayerExists(const std::string_view id)
{
  assert(api);
  return api->LayerExists(id);
}

size_t
Skysight::AddSelectedLayer(const std::string_view id)
{
  assert(api);

  if (!api->LayerExists(id))
    return -3;  // layer don't exists

  if (api->IsSelectedLayer(id))
    return -1;  // layer is already loaded

  if (api->SelectedLayersFull())
    return -2;

  SkysightLayer *m = api->GetLayer(id);
  if (m) {
    GetSelectedLayerState(id, *m);

    api->selected_layers.push_back(*m);
    SaveSelectedLayers();
  }
  return api->selected_layers.size() - 1;
}

void
Skysight::RefreshSelectedLayer(const std::string_view id)
{
  auto layer = api->GetLayer(id);
  if (layer)
    GetSelectedLayerState(id, *layer);

}

SkysightLayer *
Skysight::GetSelectedLayer(int index)
{
#if 1
  assert(index < (int)api->selected_layers.size());
  auto &layer = api->selected_layers.at(index);

  return &layer;
#else
  return nullptr;
#endif
}

SkysightLayer *
Skysight::GetSelectedLayer(const std::string_view id)
{
  for (auto &layer : api->selected_layers)
    if (layer.id == id)
      return &layer;

  return nullptr;
}

#if 1
void
Skysight::SetSelectedLayerUpdateState(const std::string_view id, bool state)
{
  auto layer = api->GetLayer(id);
  if (layer)
    layer->updating = state;
}
#endif

void
Skysight::RemoveSelectedLayer(size_t index)
{
  assert(index < api->selected_layers.size());
  api->selected_layers.erase(api->selected_layers.begin() + index);
  SaveSelectedLayers();
}

void
Skysight::RemoveSelectedLayer(const std::string_view id)
{
  for (std::vector<SkysightLayer>::const_iterator iter = api->selected_layers.begin();
    iter < api->selected_layers.end(); ++iter) {
    if (iter->id == id) {
      api->selected_layers.erase(iter);
      break;
    }
  }

  SaveSelectedLayers();
}

bool
Skysight::SelectedLayersUpdating()
{
  for (auto &layer : api->selected_layers)
    if (layer.updating) return true;

  return false;
}

size_t
Skysight::NumSelectedLayers()
{
  return api->selected_layers.size();
}

void
Skysight::SaveSelectedLayers()
{
#if 0
  StaticString<128> s;
  // api->SaveSelectedLayers(s);
  Profile::Set(ProfileKeys::SkysightSelectedLayers, s);
#else
  std::string am_list;

  if (NumSelectedLayers()) {
    for(auto &layer: api->selected_layers) {
      am_list += layer.id;
      am_list += ",";
    }
    am_list.pop_back();
  } else {
    am_list = "";
  }
  Profile::Set(ProfileKeys::SkysightSelectedLayers, am_list.c_str());
#endif
}

void
Skysight::LoadSelectedLayers()
{
#if 0
  std::string_view layer_ids =
    Profile::Get(ProfileKeys::SkysightSelectedLayers);
  std::string_view active_id = 
    Profile::Get(ProfileKeys::WeatherLayerDisplayed);
  // api->SaveSelectedLayers(layer_ids);
  if (!active_id.empty()) {
    SetActiveLayer(active_id);
  }
#else

  const char *id = Profile::Get(ProfileKeys::SkysightSelectedLayers);
  if (id == nullptr)
    return;
  std::string am_list = std::string(id);
  size_t pos;

  api->selected_layers.clear();
  while ((pos = am_list.find(",")) != std::string::npos) {
    AddSelectedLayer(am_list.substr(0, pos).c_str());
    am_list.erase(0, pos + 1);
  }
  AddSelectedLayer(am_list.c_str()); // last one

  auto d = Profile::Get(ProfileKeys::WeatherLayerDisplayed);
  if (d == nullptr)
    return;

  if (!api->IsSelectedLayer(d))
    return;

  SetActiveLayer(d);
#endif
}

bool
Skysight::IsReady([[maybe_unused]] bool force_update)
{
  if (email.empty() || password.empty() || region.empty())
    return false;

  return (NumLayers() > 0);
}

Skysight::Skysight(CurlGlobal &_curl)
{
  self = this;
  curl = &_curl;
  Init();
}

void
Skysight::Init()
{
  if (api) {
    delete api;
    api = nullptr;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  active_layer = nullptr;

#if defined(SKYSIGHT_FILE_DEBUG)
  // save in debug case an additional file in folder
  std::string _path("skysight/");
  _path += DateTime::str_now() + " ====== Start-Skysight.txt";
  auto file = fopen(LocalPath(_path).c_str(), "wb");


    if (file) {
      std::string file_text = DateTime::str_now();
      fwrite(file_text.c_str(), 1, file_text.length(), file);
      fclose(file);
    }
#endif
 
  const auto settings =
    CommonInterface::GetComputerSettings().weather.skysight;
  region = settings.region.c_str();
  email = settings.email.c_str();
  password = settings.password.c_str();

  api = new SkysightAPI(email, password, region, APIInited);
  // APIInited();
  CleanupFiles();

//  Invoke
}

void
Skysight::APIInited([[maybe_unused]] const std::string details,
  [[maybe_unused]] const bool success,
  [[maybe_unused]] const std::string layer_id,
  [[maybe_unused]] const time_t time_index)
{
  if (!self)
    return;

  if (self->api) {
    self->LoadSelectedLayers();
    self->Render(true);
  }
}


// TODO(August2111): use layer_name or layer only...
bool
Skysight::GetSelectedLayerState(const std::string_view layer_name,
  [[maybe_unused]] SkysightLayer &layer)
{
  std::string search_pattern = region + "-" + layer_name.data() + "*";
  std::vector<SkysightImageFile> img_files = ScanFolder(search_pattern);

  if (img_files.size() > 0) {
    time_t min_date = (time_t)std::numeric_limits<uint64_t>::max;
    time_t max_date = 0;
    time_t updated = 0;

    for (auto &i : img_files) {
      min_date = std::min(min_date, i.datetime);
      max_date = std::max(max_date, i.datetime);
      updated = std::max(updated, i.mtime);
    }
    auto m = GetLayer(layer_name);
    if (m) {
      m->from = min_date;
      m->to = max_date;
      m->mtime = updated;

      return true;
    }
  }

  return false;
}

std::vector<SkysightImageFile>
Skysight::ScanFolder(std::string search_string = "*.tif")
{
  // start by checking for output files
  std::vector<SkysightImageFile> file_list;

  struct SkysightFileVisitor: public File::Visitor {
    std::vector<SkysightImageFile> &file_list;
    explicit SkysightFileVisitor(std::vector<SkysightImageFile> &_file_list):
      file_list(_file_list) {}

    void Visit(Path path, Path filename) override {
      // is this a tif filename
      if (filename.EndsWithIgnoreCase(".tif")) {
        SkysightImageFile img_file = SkysightImageFile(filename, path);
        if (img_file.is_valid)
          file_list.emplace_back(img_file);
      }
    }

  } visitor(file_list);

  Directory::VisitSpecificFiles(GetLocalPath(), search_string.c_str(),
                visitor);
  return file_list;
}

void
Skysight::CleanupFiles()
{
  struct SkysightTIFVisitor: public File::Visitor {
    explicit SkysightTIFVisitor(const time_t _to): to(_to) {}
    const time_t to;
    void Visit(Path path, Path filename) override {
      SkysightImageFile img_file = SkysightImageFile(filename, path);
      if ((img_file.mtime < to) || (img_file.datetime < to ) ) {
        File::Delete(path);
      }
    }
  };

  struct SkysightFileDeleter: public File::Visitor {
    explicit SkysightFileDeleter(const time_t _to): to(_to) {}
    const time_t to;
    void Visit(Path fullpath, [[maybe_unused]] Path filename) override {
        auto mtime = File::GetTime(fullpath);
        if (mtime < to) { 
          File::Delete(fullpath);
        }
    }
  };
  char buffer[30];
  FormatISO8601(buffer, std::time(0));
  LogFmt("Time-Compare: {}, {}", buffer, DateTime::str_now());
  
  auto now = DateTime::now();
  SkysightTIFVisitor  visitor_tif(now - 24 * _ONE_HOUR);  // 1 day
  SkysightFileDeleter deleter_jpg(now - 12 * _ONE_HOUR);  // 1/2 day
  SkysightFileDeleter deleter_tmp(now -  6 * _ONE_HOUR);  // 6 hours
  SkysightFileDeleter deleter_txt(now -  _ONE_HOUR);  // 1 hour

  auto path = GetLocalPath();
  Directory::VisitSpecificFiles(path, "*.tif", visitor_tif);
  Directory::VisitSpecificFiles(path, "*.jpg", deleter_jpg);
  Directory::VisitSpecificFiles(path, "*.tmp", deleter_tmp);
  // "*.txt;*.json" is not possible...
  Directory::VisitSpecificFiles(path, "*.txt", deleter_txt);
  Directory::VisitSpecificFiles(path, "*.json", deleter_txt);
}

void
Skysight::Render([[maybe_unused]] bool force_update)
{
  if (active_layer) {
    // set by dl callback
    if (update_flag) {
      DisplayActiveLayer();
    } else {
#if 1  // start the timer?
      if (api && api->QueueIsEmpty())
        api->TimerInvoke();
#endif
    }
  }
}

time_t
Skysight::GetForecastTime(time_t time)
{
  auto forecast_time = (time / _HALFHOUR + 1) * _HALFHOUR;
  // is forecast_time plausible?
  return forecast_time;
}

bool
Skysight::SetActiveLayer(const std::string_view id,
                         time_t forecast_time)
{
  if (api->IsSelectedLayer(id)) {
    active_layer = api->GetLayer(id);
    if (active_layer) {
      active_layer->forecast_time = forecast_time;
      if (api)
        api->TimerInvoke();
      return true;
    }
  }
  return false;
}

void
Skysight::DownloadComplete([[maybe_unused]] const std::string details,
  const bool success,  const std::string layer_id,
  [[maybe_unused]] const time_t time_index)
{
  if (!self)
    return;

  self->SetSelectedLayerUpdateState(layer_id, false);
  self->RefreshSelectedLayer(layer_id);

  if (success && (self->GetActiveLayerId() == layer_id.c_str())) {
    if (!self->update_flag && self->api->QueueIsLastJob()) {
      self->update_flag = true;
      GlueMapWindow *map_window = UIGlobals::GetMapIfActive();
      if (map_window)
        map_window->DeferRedraw();

      // CommonInterface::main_window->SendGPSUpdate(); // which one is better?
      CommonInterface::main_window->SendCalculatedUpdate();
    }
  }
}

void
Skysight::OnCalculatedUpdate(const MoreData &basic,
                 [[maybe_unused]] const DerivedInfo &calculated)
{
  // maintain current time -- for use in replays etc.
  // Cannot be accessed directly from child threads
  curr_time = basic.date_time_utc;
}

BrokenDateTime
Skysight::GetNow(bool use_system_time)
{
  if (use_system_time)
    return BrokenDateTime::NowUTC();

  return (curr_time.IsPlausible()) ? curr_time : BrokenDateTime::NowUTC();
}

void
Skysight::MapOverlayReset()
{
  auto *map = UIGlobals::GetMap();
  if (map)
    for (uint16_t i = 0; i < 9; i++) map->SetOverlay(i, nullptr);
  overlay_index = 0;
}

void
Skysight::DeactivateLayer()
{
  active_layer = nullptr;
  MapOverlayReset();
  Profile::Set(ProfileKeys::WeatherLayerDisplayed, "");
}

bool
Skysight::SetLayerActive(const std::string_view id)
{
  update_flag = false;

  if (id.empty()) {
    DeactivateLayer();
    return false;
  }

  if (!api->IsSelectedLayer(id))
    return false;

  Profile::Set(ProfileKeys::WeatherLayerDisplayed, id.data());

  if (!SetActiveLayer(id))
    return false;

  return true;  // DisplayActiveLayer();
}

bool
Skysight::ForecastActiveLayer()
{
  // TODO: We're only searching w a max offset of 1 hr, simplify this!
  AllocatedPath filename;
  bool found = false;

  constexpr time_t preview = _10MINUTES;
  time_t test_time = ((DateTime::now() + preview) / _HALFHOUR + 1) * _HALFHOUR;

  constexpr auto max_steps = 3;

  for (int j = 0; !found && (j < max_steps); j++) {
    filename = api->GetPath(SkysightCallType::Image, active_layer->id,
        test_time);

    if (File::Exists(filename)) {
      // needed for (selected) object view in map
      active_layer->forecast_time = test_time;
      return UpdateActiveLayer(filename, {0, 0, 0});
    } else {
      test_time += _HALFHOUR;  // next possible forecast
    }
  }
  return false;
}

bool
Skysight::UpdateActiveLayer(const Path &filename, GeoBitmap::TileData tile)
{
  if (!File::Exists(filename))
    return false;
  auto *map = UIGlobals::GetMap();
  if (map == nullptr)
    return false;

  LogFmt("SkySight::DisplayActiveLayer {}", filename.c_str());
  std::unique_ptr<MapOverlayBitmap> bmp;
  try {
    bmp.reset(new MapOverlayBitmap(filename));
    // what is with the new created MapOverlayBitmap? Where is deleting this?
  }
  catch (...) {
    LogError(std::current_exception(), "MapOverlayBitmap load error");
    return false;
  }

  if (!active_layer)
    return false;

  StaticString<256> label;
  label.Format("SkySight: %s", active_layer->name.c_str());
  if (active_layer->live_layer || !active_layer->tile_layer)
    label.AppendFormat("  (%s)", DateTime::time_str(
      active_layer->forecast_time, "%Y-%m-%d %H:%M").c_str());
  if (active_layer->tile_layer)
    label.AppendFormat(" - tile: %u (%u, %u)", tile.zoom, tile.x, tile.y);
  bmp->SetLabel(label);
  bmp->SetAlpha(active_layer->name == "satellite" ? 1.0 : 0.6);
  map->SetOverlay(overlay_index, std::move(bmp));
  if (active_layer->tile_layer)
    overlay_index++;  // increasing in tile mode only...
  update_flag = false;  // is already updated
  return true;
}
bool
Skysight::TileActiveLayer()
{
  GlueMapWindow *map_window = UIGlobals::GetMap();
  GeoBitmap::TileData base_tile;
  if (map_window) { // && map_window->IsPanning()) {
    base_tile = GeoBitmap::GetTile(map_window->VisibleProjection());
  }
  else {
    return false;
  }

  MapOverlayReset();

  auto tile = base_tile;
  for (tile.x = base_tile.x - 1; tile.x <= base_tile.x + 1; tile.x++)
    for (tile.y = base_tile.y - 1; tile.y <= base_tile.y + 1; tile.y++) {

      GeoBounds bounds = GeoBitmap::GetBounds(tile);
      bool inside = false;
      inside |= map_window->VisibleProjection().GeoVisible(bounds.GetNorthEast());
      inside |= map_window->VisibleProjection().GeoVisible(bounds.GetNorthWest());
      inside |= map_window->VisibleProjection().GeoVisible(bounds.GetSouthEast());
      inside |= map_window->VisibleProjection().GeoVisible(bounds.GetSouthWest());

      if (!inside)
        continue;

      AllocatedPath filename;
      bool found = false;

      if (!active_layer->live_layer) { // osm
        UpdateActiveLayer(api->GetPath(SkysightCallType::Tile,
          active_layer->id, 0, tile), tile);
      } else {

        time_t test_time = (DateTime::now() / _10MINUTES) * _10MINUTES;

        auto max_steps = 3;

        for (int j = 0; !found && (j < max_steps); j++) {
          filename = api->GetPath(SkysightCallType::Tile, active_layer->id,
              test_time, tile);

          if (File::Exists(filename)) {
            // needed for (selected) object view in map
            active_layer->forecast_time = test_time;
            UpdateActiveLayer(filename, tile);
            break;
          } else {
            if (active_layer->live_layer)
              test_time -= _10MINUTES;  // previous live picture
            else
              test_time += _HALFHOUR;  // next possible forecast
          }
        }
      }
    }
  return false;
}

bool
Skysight::DisplayActiveLayer()
{
  if (!active_layer)
    return false;

  AllocatedPath filename;
  bool found = false;
  if (active_layer->tile_layer) {
    // no time stamp...
    return TileActiveLayer();
  } else if (active_layer->id.starts_with("osm")) {
    // no time stamp...
    filename = api->GetPath(SkysightCallType::Image, active_layer->id, 0);
    found = File::Exists(filename);
  } else {
    // TODO: We're only searching w a max offset of 1 hr, simplify this!
    found = ForecastActiveLayer();
  }
  
  return found;
}
