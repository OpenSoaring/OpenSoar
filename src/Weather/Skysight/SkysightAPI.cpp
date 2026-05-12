// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Weather/Skysight/SkysightAPI.hpp"
#include "Weather/Skysight/Skysight.hpp"
#include "Weather/Skysight/SkySightRequest.hpp"
#include "Weather/Skysight/SkysightRegions.hpp"
#include "Weather/Skysight/Layers.hpp"
#ifdef SKYSIGHT_FORECAST 
# include "CDFDecoder.hpp"
# include "io/ZipArchive.hpp"
#endif  // SKYSIGHT_FORECAST 


#include "co/Task.hxx"
#include "Weather/Skysight/SkySightRequest.hpp"
#include "Operation/PluggableOperationEnvironment.hpp"
#include "net/http/Init.hpp"

#include "util/StaticString.hxx"

#include "MapWindow/GlueMapWindow.hpp"
#include "UIGlobals.hpp"

#include "LogFile.hpp"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"

#include "Operation/Operation.hpp"
#include "io/BufferedReader.hxx"
#include "io/FileLineReader.hpp"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Request.hxx"
#include "time/DateTime.hpp"
#include "ui/canvas/custom/GeoBitmap.hpp"
#include "Geo/GeoBounds.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <list>
#include <thread>

#include <windef.h>  // for MAX_PATH

SkysightAPI *SkysightAPI::self;

#ifdef _DEBUG
static constexpr uint32_t forecast_count = 2;
#else
static constexpr uint32_t forecast_count = 6;
#endif

#ifdef THREAD_TIMER_START
// =======================================================
std::chrono::time_point<std::chrono::steady_clock> tnow;
std::chrono::time_point<std::chrono::steady_clock> tstart;
std::chrono::time_point<std::chrono::steady_clock> tend;
bool do_stop = false;
bool timer_running = false;
bool call_delayed_function = true;

std::thread thread_runner;

void the_function_to_delay(void)
{
//  std::cout << get_hms() << "The function (to delay) has ran!" << std::endl;
  do_stop = true;
}

void sleeper_threadfunc(std::function<void(void)> func) {
  //std::this_thread::sleep_until(tend);
  while (timer_running) {
    tnow = std::chrono::steady_clock::now();
    if (tnow >= tend) {
      timer_running = false;
      break; // exit while loop
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (call_delayed_function) {
    func();
    // the_function_to_delay();
  }
}

// inspired by https://stackoverflow.com/a/43373364/6197439
void timer_start(std::function<void(void)> func, uint64_t ms)
{
  timer_running = true;
  call_delayed_function = true;
  tstart = std::chrono::steady_clock::now();
  tend = tstart + std::chrono::milliseconds(ms);
  thread_runner = std::thread(sleeper_threadfunc, func);
}

#endif  // THREAD_TIMER_START

AllocatedPath SkysightAPI::cache_path = nullptr;

SkysightAPI::~SkysightAPI()
{
  delete co_request;
  co_request = nullptr;
  LogFmt("SkysightAPI::~SkysightAPI {}", timer.IsActive());
  timer.Cancel();
}

SkySight::Layer *
SkysightAPI::GetLayer(size_t index)
{
  assert(index < layers_vector.size());
  auto &layer = layers_vector.at(index);

  return &layer;
}

SkySight::Layer *
SkysightAPI::GetLayer(const std::string_view id)
{
  auto layer  = layers.find(id.data());
  return layer != layers.end() ? layer->second : nullptr;
}

size_t
SkysightAPI::AddSelectedLayer(const std::string_view id)
{
  selected_layers.push_back(GetLayer(id));
  return selected_layers.size() - 1;
}

bool
SkysightAPI::LayerExists(const std::string_view id)
{
  auto layer = GetLayer(id);
  return (layer != nullptr);
}

size_t
SkysightAPI::NumLayers()
{
  return layers_vector.size();
}

const std::string
SkysightAPI::GetUrl(SkysightCallType type, const std::string_view layer_id,
  const time_t from, const GeoBitmap::TileData tile)
{
  StaticString<256> url;
  switch (type) {
    case SkysightCallType::DataDetails:
      url.Format("%s/data?region_id=%s&layer_ids=%s&from_time=%llu", 
        SKYSIGHTAPI_BASE_URL.c_str(), region.c_str(), layer_id.data(), from);
      break;
    case SkysightCallType::Tile: {
#ifdef SKYSIGHT_LIVE
      if (layer_id.starts_with("osm"))
        url.Format("%s/", OSM_BASE_URL.data());
      else {
        url.Format("%s/%s/", SKYSIGHTAPI_BASE_URL.data(), layer_id.data());
      }
      url.AppendFormat("%u/%u/%u", tile.zoom, tile.x, tile.y);
      // example: https://tile.openstreetmap.org/11/1103/685.png
      if (layer_id.starts_with("osm"))
        url.append(".png");
      else {
        auto last_time = (from / TEN_MINUTES) * TEN_MINUTES;
        url.AppendFormat("/%s", DateTime::time_str(last_time, "%Y/%m/%d/%H%M")
                        .c_str());
      }
#else
    // caller should already have an URL
#endif
    }
    break;
    default:
      break;
  }
  return url.c_str();
}

AllocatedPath
SkysightAPI::GetPath(SkysightCallType type, const std::string_view layer_id,
                     const time_t fctime, const GeoBitmap::TileData tile)
{
  StaticString<256> filename;
  switch (type) {
  case SkysightCallType::Regions:
    filename.Format("regions.json");
    break;
  case SkysightCallType::Layers:
    filename.Format("layers.json");
    break;
  case SkysightCallType::LastUpdates:
    filename.Format("last_updated.json");
    break;
  case SkysightCallType::DataDetails:
    filename.Format("datalinks-%s-%s-%llu", region.c_str(), 
      layer_id.data(), fctime);
    return AllocatedPath(filename.data());  // w/0 Cache-Path..
  case SkysightCallType::Data: {
    auto layer = GetLayer(layer_id);
    filename.Format("%s-%s-%llu-%llu-%s.zip", region.c_str(), layer_id.data(),
      fctime,
      layer ? layer->update_time : 0, 
      DateTime::time_str(fctime, "%d-%H%M").c_str());
    }
    break;
  case SkysightCallType::Tile: {
    std::stringstream s;
    std::string_view servername;
    bool is_osm = layer_id.starts_with("osm");
    servername = is_osm ? OSM_BASE_URL : SKYSIGHTAPI_BASE_URL;
    auto path = GetUrl(type, layer_id, fctime, tile)
      .substr(servername.length() + 1);
    // substitute '/' with '-':
    for (auto &ch : path)
      if (ch == '/') ch = '-';

    if (is_osm) {
      auto osm_path = AllocatedPath::Build(::GetCachePath(), "osm");
      if (!Directory::Exists(osm_path)) {
        Directory::Create(AllocatedPath::Build(::GetCachePath(), "osm"));
      }
      Directory::Create(AllocatedPath::Build(::GetCachePath(), "osmX"));
      return AllocatedPath::Build(::GetCachePath(), filename);
    } else {
      filename = path + (GetLayer(layer_id)->live_layer ? ".jpg" : ".nc");
    }
  }
    break;
  case SkysightCallType::Image:
    if (GetLayer(layer_id)->tile_layer)
      return GetPath(SkysightCallType::Tile, layer_id, fctime)
                    .WithSuffix(".jpg");
    else 
      return GetPath(SkysightCallType::Data, layer_id, fctime)
                     .WithSuffix(".tiff");   // and not ".zip"!
  default:
    break;
  }
  return AllocatedPath::Build(cache_path, filename);
}

void
SkysightAPI::InitAPI(std::string_view email, std::string_view password,
  std::string_view _region, [[maybe_unused]] SkysightCallback cb)
{
  self = this;
  inited_regions = false;
  inited_layers = false;
  inited_lastupdates = false;
  LoadDefaultRegions(_region);

  if (timer.IsActive()) {
    timer.Cancel();
  }

  if (email.empty() || password.empty())
    // w/o eMail and password no communication possible
    return;

  co_request = new SkysightRequest(this, email, password);

  // Check for maintenance actions every minute - if co_request active only
  if (co_request)
    timer.Schedule(std::chrono::minutes(1));
}

bool
SkysightAPI::IsInited()
{
  return inited_regions && inited_layers && inited_lastupdates;
}

void
SkysightAPI::LoadDefaultRegions(const std::string_view select_region)
{
  for (auto r = skysight_region_defaults; r->id != nullptr; ++r) {
    regions.emplace(std::pair<std::string, std::string>(r->id, r->name));
  }

  region = select_region;
  if (region.empty() || regions.find(region) == regions.end()) {
    region = "EUROPE";  // the 'standard' region
  }

}

bool
SkysightAPI::UpdateRegions(const boost::json::value &_regions)
{
  bool success = false;
#if defined(_DEBUG)
  LogFmt("{}: start",__func__);
#endif

  regions.clear();
  for (auto &_region : _regions.as_array()) {
    SkySight::Region region;
    region.id = _region.at("id").get_string().data();
    region.name = _region.at("name").get_string().data();
    region.projection = _region.at("projection").get_string().data();
    regions[region.id] = region;
  }
  if (!regions.empty()) {
    success = true;
    inited_regions = true;
    std::string str;
    int i = 0;
    for (auto &_region : regions) {
      str += _region.second.id + ", ";
      if (++i > 3)
        break;
    }

    LogFmt("SkySight::Regions count = {}: {}... ",
      regions.size(), str);
#ifdef SKYSIGHT_TIME_DELAY
    last_request = 0;  // the next request can be started
#endif
    TimerInvoke();
  }
#if defined(_DEBUG)
  LogFmt("{}: finished",__func__);
#endif
  return success;
}

bool
SkysightAPI::UpdateLayers(const boost::json::value &_layers)
{
#if defined(_DEBUG)
  LogFmt("{}: start",__func__);
#endif
  layers_vector.clear();
  bool success = false;

#ifdef SKYSIGHT_LIVE
  layers_vector.push_back(SkySight::Layer(
    "satellite", "Satellite", "live satellite images", true, true, 8));
  layers_vector[0].alpha = 1.0; // 1st layer is 'Satellite'
  layers_vector.push_back(SkySight::Layer(
    "rain", "Rain", "live rain layer", true, true, 8));
  layers_vector.push_back(SkySight::Layer(
    "osm", "Open Street Map", "OSM layer", false, true));
  layers_vector[2].alpha = 1.0; // 3rd layer is 'OSM'
#endif

  for (auto &_layer : _layers.as_array()) {
    SkySight::Layer layer(
      _layer.at("id").get_string().data(),
      _layer.at("name").get_string().data(),
      _layer.at("description").get_string().data()
    );
    layer.projection = _layer.at("projection").get_string().data();
    layer.data_type = _layer.at("data_type").get_string().data();

      auto _legend = _layer.at("legend");
      [[maybe_unused]] auto color_mode = _legend.at("color_mode").get_string().data();
      [[maybe_unused]] auto units = _legend.at("units").get_string().data();


      for (auto _colors : _legend.at("colors").get_array()) {
        [[maybe_unused]] auto name = _colors.at("name").get_string().data();
        auto value = std::atof(_colors.at("value").get_string().data());
        auto _legend_colors = _colors.at("color").as_array();
        layer.legend[value] = LegendColor(
          _legend_colors[0].to_number<uint8_t>(),
          _legend_colors[1].to_number<uint8_t>(),
          _legend_colors[2].to_number<uint8_t>()
        );
    }

    layers_vector.push_back(layer);
  }

  if (!layers_vector.empty()) {
    success = true;
    inited_layers = success;
    std::string str;
    int i = 0;
	/* TODO(August2111): do I have to use the reference or better the layer?
	 * It is nor clear about the behaviour 
	 */
    for (auto &layer : layers_vector) {
      if (i++ < 3)
        str += layer.id + ", ";
      //if (&layer)
      layers[layer.id] = &layer;
    }

    LogFmt("SkySight::Layers count = {}: {}... ",
      layers_vector.size(), str);
#ifdef SKYSIGHT_TIME_DELAY
    last_request = 0;  // the next request can be started
#endif

    if (Skysight::GetSkysight()->NumSelectedLayers() == 0)
      Skysight::GetSkysight()->LoadSelectedLayers();
    if(co_request)
       TimerInvoke();
  }

#if defined(_DEBUG)
  LogFmt("{}: finished",__func__);
#endif
  return success;
}

bool
SkysightAPI::UpdateLastUpdates(const boost::json::value &_layers)
{
#if defined(_DEBUG)
  LogFmt("{}: finished",__func__);;
#endif
  bool success = false;
  auto active_layer = Skysight::GetActiveLayer();
  auto now = DateTime::now();
  std::list<SkySight::Layer *> update_layers;
  
  for (auto &_layer : _layers.as_array()) {
    std::string layer_id = _layer.at("layer_id").get_string().data();
    auto layer = GetLayer(layer_id);
    if (layer) {
      try {
        time_t update_time = atoll(_layer.at("time").as_string().data());
        if (update_time > layer->update_time) {
          layer->update_time = update_time;  // now;
          layer->dataname.clear();  // new update time, so clear old dataname
          success |= true;
          if (active_layer && layer_id == active_layer->id)
            update_layers.push_front(layer); // _id);
          else if (IsSelectedLayer(layer_id))
            update_layers.push_back(layer);  // _id);
        }
      }
      catch (std::exception &e) {
        LogError(std::current_exception(), e.what());
      }
    }
  }
  for (const auto layer : update_layers) {
    LogFmt("New Forecast request for {} with {}!", 
      layer->id, layer->update_time);
    GetData(SkysightCallType::DataDetails, layer->id, layer->update_time, now);
    layer->forecast_links.clear();  // clear old links, new ones will be added
  }

  if (!layers_vector.empty()) {
    // success = true;
    inited_lastupdates = true;
    lastupdates_time = now;
#ifdef SKYSIGHT_TIME_DELAY
    last_request = 0;  // the next request can be started
#endif
#ifdef _DEBUG  // logfile output
    int i = 0;
    std::string str;
    for (auto &layer : layers) if (layer.second) {
      str += DateTime::time_str(layer.second->update_time, "%H:%M:%S") + ", ";
      if (++i > 3)  // only for 3 layers as example...
        break;
    }
    LogFmt("SkySight::LastUpdated count = {}: {}... ", layers.size(), str);
 #endif
    TimerInvoke();
  }
#if defined(_DEBUG)
  LogFmt("{}: finished",__func__);
#endif
  return success;
}

bool
SkysightAPI::UpdateDatafiles(const boost::json::value &_datafiles)
{
  bool success = false;
  // auto active_layer = Skysight::GetActiveLayer();
  // if (active_layer) {
  //  std::string_view active_layer_id = active_layer->id;

  
  auto max_time = DateTime::now() + ONLINE_FORECAST;
  
  for (auto &_filepoint : _datafiles.as_array()) {
      std::string layer_id = _filepoint.at("layer_id").get_string().data();
      time_t time_index = _filepoint.at("time").to_number<time_t>();
      std::string link = _filepoint.at("link").get_string().data();

      if ((time_index > 0) && !link.empty()) {
        // save all links (not only the relevant
        auto layer = GetLayer(layer_id);
        if (layer != nullptr) {
          layer->forecast_links[time_index] = link;
          if (time_index <= max_time) {
              success = GetData(SkysightCallType::Data, layer_id, 
                time_index, layer->update_time, link);
          }
        }
      }
    }
  // }
  return success;
}

#ifdef SKYSIGHT_FORECAST
bool
SkysightAPI::BuildForecastTiff(const Path filename)
{
  bool success = false;
  auto zip_file = filename.WithSuffix(".zip");
  auto nc_file = filename.WithSuffix(".nc");
  if (!File::Exists(nc_file) &&
    // do it one time only
    File::Exists(zip_file)) {
    ZipIO::UnzipSingleFile(zip_file, nc_file);
    if (File::Exists(nc_file)) {
      char buffer[8];
      File::ReadString(nc_file, buffer, sizeof(buffer));  // read buffer
      if (strncmp(buffer, "CDF", 3) == 0) {
        // and now it is a CDF file
        CallCDFDecoder(Skysight::GetActiveLayer(), nc_file.c_str(),
          filename.c_str(), Skysight::RefreshDisplay);
        // file found and build tiff started, but not yet finished:
        success = true;
      }
    }
  }
  return success;
}

void
SkysightAPI::CallCDFDecoder(const SkySight::Layer *layer, 
  const std::string_view &cdf_file,
  const std::string_view& output_img, const SkysightCallback _callback)
{
  queue.AddDecodeJob(std::make_unique<CDFDecoder>(
    cdf_file.data(), output_img.data(), layer->id.c_str(),
    layer->legend, _callback));
}
#endif  // SKYSIGHT_FORECAST 

bool
SkysightAPI::GetTileData(const std::string_view layer_id,
    const time_t from, [[maybe_unused]] const time_t to,
    SkysightCallback cb, [[maybe_unused]] bool force_recache)
{

  GlueMapWindow *map_window = UIGlobals::GetMap();
  GeoBitmap::TileData base_tile;
  auto layer = GetLayer(layer_id);

  if (layer->live_layer && !IsLoggedIn()) {
      // inject a login request at the front of the queue
      SkysightAPI::GenerateLoginRequest();
  }

  const SkysightCallType type = SkysightCallType::Tile;
  if (map_window && map_window->IsVisible()) {
      base_tile = GeoBitmap::GetTile(map_window->VisibleProjection(),
      layer->zoom_min, layer->zoom_max);
  } else {
    return false;
  }

  auto tile = base_tile;
  auto map_bounds = map_window->VisibleProjection().GetScreenBounds();

  if (!map_bounds.Check())
    return false;

  // o: 0 -> 1 tile, or 1 -> 9 tiles, (or 2 -> 25 tiles?)
  constexpr uint16_t o = TILE_RANGE_OFFSET;

  for (tile.x = base_tile.x - o; tile.x <= base_tile.x + o; tile.x++)
    for (tile.y = base_tile.y - o; tile.y <= base_tile.y + o; tile.y++) {

      if (!GeoBitmap::GetBounds(tile).Overlaps(map_bounds))
          continue;  // w/o overlapping no Request!
      const std::string url = GetUrl(type, layer_id, from, tile);
      const auto path = GetPath(type, layer_id, from, tile);

      if (!File::Exists(path)) {
        if (co_request) {
          co_request->DownloadFile(url, path, SKYSIGHT_LIVE_REQUEST);
        }
      }
    }
  return true;
}

bool
SkysightAPI::GetData(SkysightCallType type, const std::string_view layer_id,
    const time_t from, [[maybe_unused]] const time_t to,
    [[maybe_unused]] const std::string_view link,
    SkysightCallback cb, [[maybe_unused]] bool force_recache)
{
  const std::string url = link.empty() || !link.starts_with("https://") ? 
       GetUrl(type, layer_id, from) : std::string(link);
  const auto path = GetPath(type, layer_id, from);

  switch (type) {
    case SkysightCallType::Tile:
      assert(false);
      return false;
    case SkysightCallType::Data:
      if (!File::Exists(path)) {
        if (co_request)
          co_request->DownloadFile(url, path, SKYSIGHT_FORECAST_REQUEST);
        return true;
      }
      break;
    case SkysightCallType::DataDetails: {
      std::stringstream url_part;
      url_part << "data?region_id=" << region << "&layer_ids=" << layer_id;
      url_part << "&from_time=" << from;
      auto layer = GetLayer(layer_id);
      if (layer->dataname.empty())
        layer->dataname = GetPath(SkysightCallType::DataDetails,
          layer_id, from).c_str();
      if (co_request) {
        co_request->RequestJson(layer->dataname, url_part.str());
      }
      return true;
    }
      break;
    default:
      break;
  }

  return true;
}

bool
SkysightAPI::CacheAvailable(Path path, SkysightCallType calltype,
                            const char *const layer)
{
  time_t layer_updated = 0;
  if (layer) {
    layer_updated = GetLayer(layer)->update_time;
  }

  if (File::Exists(path)) {
    switch (calltype) {
    case SkysightCallType::Image:
      if (!layer)
	      return false;
      return (layer_updated <= File::GetTime(path));
      break;
    case SkysightCallType::DataDetails:
      // these aren't cached to disk ???
      return false;
    default:
      return false;
      break;
    }
  }

  return false;
}

bool
SkysightAPI::GetResult(const SkysightRequestArgs &args,
                       const std::string result,
                       boost::property_tree::ptree &output)
{
  try {
    if (!args.path.empty()) {
      boost::property_tree::read_json(result.c_str(), output);
    } else {
      std::stringstream result_stream(result);
      boost::property_tree::read_json(result_stream, output);
    }
  } catch(const std::exception &exc) {
    return false;
  }
  return true;
}

void
SkysightAPI::GenerateLoginRequest()
{
  if (!self)
    return;

  self->co_request->RequestCredentialKey();
}

void
SkysightAPI::MakeCallback(SkysightCallback cb, const std::string &&details,
                          const bool success, const std::string &&layer,
                          const time_t time_index)
{
  if (cb)
    cb(details.c_str(), success, layer.c_str(), time_index);
}

void
SkysightAPI::TimerInvoke()
{
  timer.Invoke();
}

bool
SkysightAPI::IsLoggedIn() {
  return co_request && co_request->IsLoggedIn();
}

/*
 * ******   PRESELECTED LAYERS ************
 */
bool
SkysightAPI::IsSelectedLayer(const std::string_view id)
{
  for (auto layer : selected_layers)
    if (layer && layer->id == id)
      return true;

  return false;
}

bool
SkysightAPI::SelectedLayersFull()
{
  return (selected_layers.size() >= SKYSIGHT_MAX_LAYERS);
}

void
SkysightAPI::OnTimer()
{
  const std::lock_guard lock{ mutex };
#if 1  // test to debug the timer behaviour - and vector.size
   LogFmt("{}: {} vs.{}", __func__,layers_vector.size(), layers.size());
#endif
  assert(layers_vector.size() == layers.size());
#if 1  // test to debug the timer behaviour - and vector.size
  // LogString("OnTimer: 0");
#endif

  timer.Schedule(std::chrono::seconds(60));

  /* TODO(August2111, 2026-04-26):
  * - One Minute before switch to new time rendering create the tiff file from
  *   zip (avoid the empty page w/o overlay (-> only active layer?)
  * -
  */

  // various maintenance actions
  auto now = DateTime::now();

#ifdef THREAD_TIMER_START
  timer_start(the_function_to_delay, 1000);
#endif  // THREAD_TIMER_START
  if (!co_request) {
    // LogString("OnTimer: No CoRequest");
    return;  // do nothing
  }

  if (!IsLoggedIn()) {
#ifdef SKYSIGHT_TIME_DELAY
    last_request = now;
#endif
    co_request->RequestCredentialKey();
    // return;
  }

  if (!inited_regions) {
#ifdef SKYSIGHT_TIME_DELAY
    if (now < last_request + 5)
        return;
    last_request = now;
#endif
    co_request->RequestJson("regions", "regions");
    return;
  }
  else if (!inited_layers) {
#ifdef SKYSIGHT_TIME_DELAY
    if (now < last_request + 5)
       return;
    last_request = now;
#endif
    co_request->RequestJson("layers", "layers?region_id=" + region);
    return;
  }
  else if (!inited_lastupdates ||
    (now > lastupdates_time + 5 * ONE_MINUTE - 10)) {
#ifdef SKYSIGHT_TIME_DELAY
    if (now < last_request + 5)
      return;
    last_request = now;
#endif
    co_request->RequestJson("last_updated", "data/last_updated?region_id=" + region);
    return;
  }
  else if (!IsInited()) {
#ifdef SKYSIGHT_TIME_DELAY
    if (now < last_request + 5)
        return;
    last_request = now;
#else
    return;
#endif
  }

  Skysight::GetSkysight()->SetUpdateFlag();  // every minute
  for (auto layer : selected_layers) {
    if (layer) {
      if (layer->tile_layer) {
        if (!layer->live_layer || IsLoggedIn()) {
          GetTileData(layer->id.c_str(), now, now,
            Skysight::DownloadComplete);
        }
#ifdef SKYSIGHT_FORECAST 
      } else {
        // forecast layer:
        // TODO(aug): every 5 or 10 minutes only 
#ifdef _DEBUG
        LogFmt("Debug: New Forecast request for {}!", layer->id);
#endif
        if (layer->forecast_links.empty()) {
          // no links available, so request the details to get the links
          if (!layer->id.empty() && layer->update_time > 0)
            GetData(SkysightCallType::DataDetails, layer->id, layer->update_time, now);

        } else {
          for (time_t time_index = ((now + HALF_HOUR) / HALF_HOUR) * HALF_HOUR;
            time_index <= now + ONLINE_FORECAST; time_index += HALF_HOUR) {
            if (layer->forecast_links.find(time_index) != layer->forecast_links.end()) {
              bool success = GetData(SkysightCallType::Data, layer->id,
                time_index, layer->update_time, layer->forecast_links[time_index]);
              // if (success && layer->update_time < time_index)
              //  layer->update_time = time_index;
            }
          }
        }
        if (layer == Skysight::GetActiveLayer()) {
          // only for the active layer:
          time_t test_time = DateTime::TimeRaster(DateTime::now() + TEN_MINUTES,
            HALF_HOUR, 1);
          auto diff_time = test_time - now - FORECAST_OFFSET;
#ifdef _DEBUG
          LogFmt("TimeCheck: testtime = {}, diff = {}", DateTime::time_str(test_time), diff_time);
#endif
          if (diff_time >= 0 && diff_time < 2 * ONE_MINUTE) {
            test_time += HALF_HOUR;  // switch to next forecast time
            auto display_file = GetPath(SkysightCallType::Image, layer->id, test_time);
            if (!File::Exists(display_file)) {
              BuildForecastTiff(display_file);
#ifdef _DEBUG
              // LogFmt("Create new Tiff:  {}", display_file.c_str());
#endif
            }
          }
        }
#endif  // SKYSIGHT_FORECAST
      }  // layer->tile_layer
    }
  }
}

