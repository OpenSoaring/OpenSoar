// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Skysight.hpp"
#include "system/ConvertPathName.hpp"
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
 * -- overlay only shows following render -- no way to trigger from child thread
 * -- no transparent bg on overlay on android
 *
 * --- for release ----
 * - Use SkysightImageFile elsewhere instead of recalculating forecast time, move to separate imp file
 * - clean up libs
 * - rebase on latest master, clean up
 * - move cache trimming to API?
 * - clean up layers/activelayers/displayed_layer -- inheritance rather than pointer
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

  mtime = std::chrono::system_clock::to_time_t(File::GetLastModification(fullpath));

  region = reg;
  layer = met;
  is_valid = true;
}

/*
 * ******   ACTIVE METRICS ************
 *
 */
bool
Skysight::IsActiveLayer(const char *const id)
{
  for (auto &i : active_layers)
    if (!i.layer->id.compare(id))
      return true;

  return false;
}

bool
Skysight::ActiveLayersFull()
{
  return (active_layers.size() >= SKYSIGHT_MAX_METRICS);
}

int
Skysight::AddActiveLayer(const char *const id)
{
  bool layer_exists = false;
  std::vector<SkysightLayer>::iterator i;
  for (i = api->layers.begin(); i < api->layers.end(); ++i)
    if (!i->id.compare(id)) {
      layer_exists = true;
      break;
    }
  if (!layer_exists)
    return -3;

  if (IsActiveLayer(id))
    return -1;

  if (ActiveLayersFull())
    return -2;

  SkysightActiveLayer m = SkysightActiveLayer(&(*i), 0, 0, 0);

  GetActiveLayerState(id, m);

  active_layers.push_back(m);
  SaveActiveLayers();
  return active_layers.size() - 1;
}

void
Skysight::RefreshActiveLayer(std::string id)
{
  std::vector<SkysightActiveLayer>::iterator i;
  for (i = active_layers.begin(); i < active_layers.end(); ++i) {
    if (!i->layer->id.compare(id)) {
      GetActiveLayerState(id, (*i));
    }
  }
}

SkysightActiveLayer
Skysight::GetActiveLayer(int index)
{
  assert(index < (int)active_layers.size());
  auto &i = active_layers.at(index);

  return i;
}

SkysightActiveLayer
Skysight::GetActiveLayer(const std::string id)
{
  std::vector<SkysightActiveLayer>::iterator i;

  for (i = active_layers.begin(); i < active_layers.end(); ++i)
    if (!i->layer->id.compare(id)) {
      return (*i);
    }

  assert(i < active_layers.end());

  return (*i);
}

void
Skysight::SetActveLayerUpdateState(const std::string id, bool state)
{
  for (auto &i: active_layers) {
    if (!i.layer->id.compare(id)) {
      i.updating = state;
      return;
    }
  }
}

void
Skysight::RemoveActiveLayer(int index)
{
  assert(index < (int)active_layers.size());
  active_layers.erase(active_layers.begin() + index);
  SaveActiveLayers();
}

void
Skysight::RemoveActiveLayer(const std::string id)
{
  std::vector<SkysightActiveLayer>::iterator i;

  for (i = active_layers.begin(); i < active_layers.end(); ++i) {
    if (i->layer->id == id)
      active_layers.erase(i);
  }
  SaveActiveLayers();
}

bool
Skysight::ActiveLayersUpdating()
{
  for (auto i: active_layers)
    if (i.updating) return true;

  return false;
}

int
Skysight::NumActiveLayers()
{
  return (int)active_layers.size();
}

void
Skysight::SaveActiveLayers()
{
  std::string am_list;

  if (NumActiveLayers()) {
    for(auto &i: active_layers) {
      am_list += i.layer->id;
      am_list += ",";
    }
    am_list.pop_back();
  } else {
    am_list = "";
  }

  Profile::Set(ProfileKeys::SkysightActiveLayers, am_list.c_str());
}

void
Skysight::LoadActiveLayers()
{
  active_layers.clear();

  const char *s = Profile::Get(ProfileKeys::SkysightActiveLayers);
  if (s == NULL)
    return;
  std::string am_list = std::string(s);
  size_t pos;
  while ((pos = am_list.find(",")) != std::string::npos) {
    AddActiveLayer(am_list.substr(0, pos).c_str());
    am_list.erase(0, pos + 1);
  }
  AddActiveLayer(am_list.c_str()); // last one

  const char *const d = Profile::Get(ProfileKeys::WeatherLayerDisplayed);
  if (d == NULL)
    return;

  if (!IsActiveLayer(d))
    return;

  SetDisplayedLayer(d);
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
 
  const auto settings = CommonInterface::GetComputerSettings().weather.skysight;
  region = settings.region.c_str();
  email = settings.email.c_str();
  password = settings.password.c_str();

  api = new SkysightAPI(email, password, region, APIInited);
  CleanupFiles();
}

void
Skysight::APIInited([[maybe_unused]] const std::string details, [[maybe_unused]] const bool success,
            [[maybe_unused]] const std::string layer_id, [[maybe_unused]] const uint64_t time_index)
{
  if (!self)
    return;

  if (self->api->layers.size()) {
    self->LoadActiveLayers();
    self->Render(true);
  }
}

bool
Skysight::GetActiveLayerState(std::string layer_name, SkysightActiveLayer &m)
{
  std::string search_pattern = region + "-" + layer_name + "*";
  std::vector<SkysightImageFile> img_files = ScanFolder(search_pattern);

  if (img_files.size() > 0) {
    uint64_t min_date = (uint64_t)std::numeric_limits<uint64_t>::max;
    uint64_t max_date = 0;
    uint64_t updated = 0;

    for (auto &i: img_files) {
      min_date = std::min(min_date, i.datetime);
      max_date = std::max(max_date, i.datetime);
      updated  = std::max(updated, i.mtime);
    }
    if (LayerExists(layer_name)) {
      m.layer = GetLayer(layer_name);
      m.from = min_date;
      m.to = max_date;
      m.mtime = updated;

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
  } visitor(std::chrono::system_clock::to_time_t(Skysight::GetNow().ToTimePoint()));

  Directory::VisitSpecificFiles(GetLocalPath(), "*.tif", visitor);
}

BrokenDateTime
Skysight::FromUnixTime(uint64_t t)
{
  return api->FromUnixTime(t);
}

void
Skysight::Render(bool force_update)
{
  if (displayed_layer.layer) {
    // set by dl callback
    if (update_flag) {
      // TODO: use const char in layer rather than string/cstr
      DisplayActiveLayer(displayed_layer.layer->id.c_str());
    }
#if 0
    // Request next images
    BrokenDateTime now = Skysight::GetNow(force_update);
    if (force_update || !displayed_layer.forecast_time.IsPlausible() ||
       (!update_flag && displayed_layer < GetForecastTime(now))) {
      force_update = false;
      // TODO: use const char in layer rather than string/cstr
      api->GetImageAt(displayed_layer.layer->id.c_str(), now, now + std::chrono::seconds(60*60),
             DownloadComplete);
    }
#endif
  }
}

BrokenDateTime
Skysight::GetForecastTime(BrokenDateTime curr_time)
{
  if (!curr_time.IsPlausible())
    curr_time = Skysight::GetNow();

  if ((curr_time.minute >= 15) && (curr_time.minute < 45))
    curr_time.minute = 30;
  else if (curr_time.minute >= 45) {
    curr_time.minute = 0;
    curr_time = curr_time + std::chrono::seconds(60*60);
  }
  else if (curr_time.minute < 15)
    curr_time.minute = 0;

  curr_time.second = 0;
  return curr_time;
}

bool
Skysight::SetDisplayedLayer(const char *const id,
                 BrokenDateTime forecast_time)
{
  if (!IsActiveLayer(id))
    return false;

  SkysightLayer *m = api->GetLayer(id);
  displayed_layer = DisplayedLayer(m, forecast_time);

  return true;
}

void
Skysight::DownloadComplete([[maybe_unused]] const std::string details, const bool success,
                const std::string layer_id, [[maybe_unused]] const uint64_t time_index)
{
  if (!self)
    return;

  self->SetActveLayerUpdateState(layer_id, false);
  self->RefreshActiveLayer(layer_id);

  if (success && (self->displayed_layer == layer_id.c_str()))
    self->update_flag = true;
}

bool
Skysight::DownloadActiveLayer(std::string id = "*")
{
  BrokenDateTime now = Skysight::GetNow();
  if (id == "*") {
    for (auto &i: active_layers) {
      SetActveLayerUpdateState(i.layer->id, true);
#if SKYSIGHT_DEBUG
      // reduce download request for debug!
      api->GetImageAt(i.layer->id.c_str(), now, now + std::chrono::hours(3),
#else // WIN_SKYSIGHT
      api->GetImageAt(i.layer->id.c_str(), now, now + std::chrono::hours(24),
#endif
             DownloadComplete);
    }
  } else {
    SetActveLayerUpdateState(id, true);
    api->GetImageAt(id.c_str(), now, now + std::chrono::seconds(60*60*24), DownloadComplete);
  }
  return true;
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

bool
Skysight::DisplayActiveLayer(const char *const id)
{
  update_flag = false;

  if (!id) {
    displayed_layer.clear();
    auto *map = UIGlobals::GetMap();
    if (map == nullptr)
      return false;

#if defined(HAVE_SKYSIGHT)
    map->SetOverlay(nullptr);
#endif
    Profile::Set(ProfileKeys::WeatherLayerDisplayed, "");
    return true;
  }

  if (!IsActiveLayer(id))
    return false;

  Profile::Set(ProfileKeys::WeatherLayerDisplayed, id);

  BrokenDateTime now = GetForecastTime(Skysight::GetNow());

  int offset = 0;
  uint64_t n = std::chrono::system_clock::to_time_t(now.ToTimePoint());

  uint64_t test_time;
  bool found = false;
  StaticString<256> filename;
  BrokenDateTime bdt;
  int max_offset = (60*60);

  // TODO: We're only searching w a max offset of 1 hr, simplify this!
  while (!found) {
    // look back for closest forecast first, then look forward
    for (int j=0; j <= 1; ++j) {
      test_time = n + ( offset * ((2*j)-1) );

      bdt = FromUnixTime(test_time);
      filename.Format("%s-%s-%04u%02u%02u%02u%02u.tif",
            region.c_str(), id,
            bdt.year, bdt.month,
            bdt.day, bdt.hour, bdt.minute);

      if (File::Exists(AllocatedPath::Build(GetLocalPath(),
                        filename.c_str()))) {
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

  if (!found) {
    SetDisplayedLayer(id);
    return false;
  }

  if (!SetDisplayedLayer(id, bdt))
    return false;

  auto path = AllocatedPath::Build(Skysight::GetLocalPath(), filename.c_str());
  StaticString<256> desc;
  desc.Format("Skysight: %s (%04u-%02u-%02u %02u:%02u)",
          displayed_layer.layer->name.c_str(), bdt.year, bdt.month, 
          bdt.day, bdt.hour, bdt.minute);
  std::string label = desc.c_str();

  auto *map = UIGlobals::GetMap();
  if (map == nullptr)
    return false;

#if defined(HAVE_SKYSIGHT)
  LogFormat("Skysight::DisplayActiveLayer %s", path.c_str());
  std::unique_ptr<MapOverlayBitmap> bmp;
  try {
    bmp.reset(new MapOverlayBitmap(path));
  } catch (...) {
    LogError(std::current_exception(), "MapOverlayBitmap load error");
    return false;
  }

  bmp->SetAlpha(0.6);
  bmp->SetLabel(label);
  map->SetOverlay(std::move(bmp));
  return true;
#else
  return false;
#endif
}
