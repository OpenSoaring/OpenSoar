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

#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include <ctime>
#include <chrono>

#if defined(SKYSIGHT_DEBUG)
# include <filesystem>
# include <regex>
# include <iomanip>  // put_time
# include <thread>
# include <fmt/format.h>
# ifdef USE_STD_FORMAT
#   include <format>
# endif
#endif

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

  std::string dt = rem.substr(p+1);
  unsigned yy = stoi(dt.substr(0, 4));
  unsigned mm = stoi(dt.substr(4, 2));
  unsigned dd = stoi(dt.substr(6, 2));
  unsigned hh = stoi(dt.substr(8, 2));
  unsigned ii = stoi(dt.substr(10, 2));

  BrokenDateTime d = BrokenDateTime(yy, mm, dd, hh, ii);
  if (!d.IsPlausible())
    return;

  datetime = std::chrono::system_clock::to_time_t(d.ToTimePoint());

  mtime = std::chrono::system_clock::to_time_t(
    File::GetLastModification(fullpath));

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
  api->selected_layers.clear();

  const char *id = Profile::Get(ProfileKeys::SkysightSelectedLayers);
  if (id == nullptr)
    return;
  std::string am_list = std::string(id);
  size_t pos;
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
#ifdef USE_STD_FORMAT
  auto tp = std::chrono::system_clock::now();
#else
  std::time_t now = std::time(0); // get time now
#endif // USE_STD_FORMAT
      std::stringstream _path;
  _path << "skysight/" <<
#ifdef USE_STD_FORMAT
      std::format("{:%Y%m%d_%H%M%S}", floor<std::chrono::seconds>(tp))
#else  // USE_STD_FORMAT
      std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S")
#endif // USE_STD_FORMAT   
    << " ====== Start-Skysight.txt";
  AllocatedPath path = LocalPath(_path.str().c_str());
  auto file = fopen(path.c_str(), "wb");

  std::stringstream file_text;
  file_text <<
#ifdef USE_STD_FORMAT
      std::format("{:%Y%m%d_%H%M%S}", floor<std::chrono::milliseconds>(tp));
#else  // USE_STD_FORMAT
      std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S");
#endif // USE_STD_FORMAT
    if (file) {
    fwrite(file_text.str().c_str(), 1, file_text.str().length(), file);
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
  [[maybe_unused]] const uint64_t time_index)
{
  if (!self)
    return;

  if (self->api && self->api->layers_vector.size()) {
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
    uint64_t min_date = (uint64_t)std::numeric_limits<uint64_t>::max;
    uint64_t max_date = 0;
    uint64_t updated = 0;

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
  struct SkysightFileVisitor: public File::Visitor {
    explicit SkysightFileVisitor(const uint64_t _to): to(_to) {}
    const uint64_t to;
    void Visit(Path path, Path filename) override {
      if (filename.EndsWithIgnoreCase(".tif")) {
        SkysightImageFile img_file = SkysightImageFile(filename, path);
        if ((img_file.mtime <= (to - (60*60*24*5))) ||
        (img_file.datetime < (to - (60*60*24))) ) {
          File::Delete(path);
        }
      }
    }
  } visitor(std::chrono::system_clock::to_time_t(
      Skysight::GetNow().ToTimePoint()));

  Directory::VisitSpecificFiles(GetLocalPath(), "*.tif", visitor);
}

void
Skysight::Render([[maybe_unused]] bool force_update)
{
  if (active_layer) {
    // set by dl callback
    if (update_flag) {
      DisplayActiveLayer();
    }
    // August2111: Rendering w/o download!
  }
}

BrokenDateTime
Skysight::GetForecastTime(BrokenDateTime curr_time)
{
  if (!curr_time.IsPlausible())
    curr_time = Skysight::GetNow();

  if ((curr_time.minute >= 15) && (curr_time.minute < 45)) {
    curr_time = curr_time + std::chrono::hours(1);
    curr_time.minute = 0;
  } else {
    if (curr_time.minute >= 45) 
      curr_time = curr_time + std::chrono::hours(1);
    // else if (curr_time.minute < 15) {}  // not necessary
    curr_time.minute = 30;
  }
  curr_time.second = 0;
  return curr_time;
}

bool
Skysight::SetActiveLayer(const std::string_view id,
                 BrokenDateTime forecast_time)
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

#if 1
void
Skysight::DownloadComplete([[maybe_unused]] const std::string details,
  const bool success,  const std::string layer_id,
  [[maybe_unused]] const uint64_t time_index)
{
  if (!self)
    return;

  self->SetSelectedLayerUpdateState(layer_id, false);
  self->RefreshSelectedLayer(layer_id);

  if (success && (self->GetActiveLayerId() == layer_id.c_str())) {
    if (!self->update_flag) {
      self->update_flag = true;
      GlueMapWindow *map_window = UIGlobals::GetMapIfActive();
      if (map_window)
        map_window->DeferRedraw();
    }
  }
}
#endif

#if 0  // def _DEBUG
bool
Skysight::DownloadSelectedLayer(const std::string_view id = "*")
{
  BrokenDateTime now = Skysight::GetNow();
  if (id == "*") {
    for (auto &layer: selected_layers) {
      SetSelectedLayerUpdateState(layer.id, true);
#if SKYSIGHT_DEBUG
      // reduce download request for debug!
      api->GetImageAt(layer.id.c_str(), now, now + std::chrono::hours(3),
#else // WIN_SKYSIGHT
      api->GetImageAt(layer.id.c_str(), now, now + std::chrono::hours(24),
#endif
             DownloadComplete);
    }
  } else {
    SetSelectedLayerUpdateState(id, true);
    api->GetImageAt(id.data(), now, now + std::chrono::seconds(60*60*24),
      DownloadComplete);
  }
  return true;
}
#endif

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
Skysight::DeactivateLayer()
{
  active_layer = nullptr;
  auto *map = UIGlobals::GetMap();
  if (map)
    map->SetOverlay(nullptr);
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
Skysight::DisplayActiveLayer()
{
  if (!active_layer)
    return false;

  BrokenDateTime now = GetForecastTime(Skysight::GetNow());

  int offset = 0;
  uint64_t n = std::chrono::system_clock::to_time_t(now.ToTimePoint());

  uint64_t test_time;
  bool found = false;
  // StaticString<256> filename;
  AllocatedPath filename;
  int max_offset = (60*60);

  // TODO: We're only searching w a max offset of 1 hr, simplify this!
  while (!found) {
    // look back for closest forecast first, then look forward
    for (int j=0; j <= 1; ++j) {
      test_time = n + ( offset * ((2*j)-1) );

      filename = api->GetPath(SkysightCallType::Image,
        active_layer->id, test_time);

      if (File::Exists(filename)) {
        // needed for (selected) object view in map
        active_layer->forecast_time = BrokenDateTime::FromUnixTimeUTC(test_time);
        found = true;
        break;
      }
      if (offset == 0)
        break;
    }
    if (!found)
      offset += (60*30);

    if (offset > max_offset)
      break;
  }

  auto *map = UIGlobals::GetMap();
  if (map == nullptr)
    return false;

  LogFormat("SkySight::DisplayActiveLayer %s", filename.c_str());
  std::unique_ptr<MapOverlayBitmap> bmp;
  try {
    bmp.reset(new MapOverlayBitmap(filename));
    // what is with the new created MapOverlayBitmap? Where is deleting this?
  } catch (...) {
    LogError(std::current_exception(), "MapOverlayBitmap load error");
    return false;
  }

  BrokenDateTime &time = active_layer->forecast_time;
  StaticString<256> label;
  label.Format("SkySight: %s (%04u-%02u-%02u %02u:%02u)",
    active_layer->name.c_str(), time.year, time.month, time.day,
    time.hour, time.minute);
  bmp->SetLabel(label); // .c_str());
  bmp->SetAlpha(0.6);
  map->SetOverlay(std::move(bmp));
  return true;
}
