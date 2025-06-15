// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Weather/Skysight/SkysightAPI.hpp"
#include "Weather/Skysight/Skysight.hpp"
#include "Weather/Skysight/SkySightRequest.hpp"
#include "Weather/Skysight/SkysightRegions.hpp"
#include "Weather/Skysight/Layers.hpp"
#ifdef SKYSIGHT_FORECAST 
# include "CDFDecoder.hpp"
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
#include "time/DateTime.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>
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
  auto layer = std::find(layers_vector.begin(), layers_vector.end(), id);
  return (layer != layers_vector.end()) ? &(*layer) :nullptr;
}

bool
SkysightAPI::LayerExists(const std::string_view id)
{
  return (std::find(layers_vector.begin(), layers_vector.end(), id)
          != layers_vector.end());
}

int
SkysightAPI::NumLayers()
{
  return (int)layers_vector.size();
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
        url.AppendFormat("/%s", DateTime::time_str(last_time, "%Y/%m/%d/%H%M").c_str());
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
  case SkysightCallType::DataDetails:
    filename.Format("datafiles-%s-%s-%s.json", region.c_str(), layer_id.data(),
       DateTime::time_str(fctime, "%d-%H%M").c_str());
     break;
  case SkysightCallType::Data: {
    auto layer = GetLayer(layer_id);
    filename.Format("%s-%s-%llu-%s.zip", region.c_str(), layer_id.data(),
      layer ? layer->last_update : 0, 
      DateTime::time_str(fctime, "%d-%H%M").c_str());
    }
    break;
  case SkysightCallType::Tile: {
    std::stringstream s;
    std::string_view servername;
    bool is_osm = layer_id.starts_with("osm");
    servername = is_osm ? OSM_BASE_URL : SKYSIGHTAPI_BASE_URL;
    auto path = GetUrl(type, layer_id, fctime, tile).substr(servername.length() + 1);
    // substitute '/' with '-':
    for (auto &ch : path)
      if (ch == '/') ch = '-';

    if (is_osm)
      filename = path;  // has already extension .png
    else
      filename = path + (GetLayer(layer_id)->live_layer ? ".jpg" : ".nc");
    }
    break;
  case SkysightCallType::Image:
    if (GetLayer(layer_id)->tile_layer)
      return GetPath(SkysightCallType::Tile, layer_id, fctime).WithSuffix(".jpg");
    else 
      return GetPath(SkysightCallType::Data, layer_id, fctime).WithSuffix(".tif");
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
SkysightAPI::ParseResponse(const std::string &&result, const bool success,
                           const SkysightRequestArgs req)
{
  if (!self)
    return;

  if (!success) {
    if (req.calltype == SkysightCallType::Login) {
      // !!! self->queue.Clear("Login error");
    } else {
      self->MakeCallback(req.cb, result.c_str() , false, req.layer.c_str(),
			 req.from);
    }
    return;
  }

  switch (req.calltype) {

    case SkysightCallType::DataDetails:
    self->ParseDataDetails(req, result);
    break;

  default:
    break;
  }
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
    last_request = 0;  // the next request can be started
    TimerInvoke();
  }
  return success;
}

bool
SkysightAPI::UpdateLayers(const boost::json::value &_layers)
{
  layers_vector.clear();
  bool success = false;

#ifdef SKYSIGHT_LIVE
  layers_vector.push_back(SkySight::Layer(
    "satellite", "Satellite", "live satellite images", true, true, 8));
  layers_vector.push_back(SkySight::Layer(
    "rain", "Rain", "live rain layer", true, true, 8));
  layers_vector.push_back(SkySight::Layer(
    "osm", "Open Street Map", "OSM layer", false, true));
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
    last_request = 0;  // the next request can be started

    if (Skysight::GetSkysight()->NumSelectedLayers() == 0)
      Skysight::GetSkysight()->LoadSelectedLayers();
    TimerInvoke();
  }

  return success;
}

bool
SkysightAPI::UpdateLastUpdates(const boost::json::value &_layers)
{
  bool success = false;
  auto active_layer = Skysight::GetActiveLayer();
  std::string_view active_layer_id;
  if (active_layer)
    active_layer_id = active_layer->id;

  for (auto &_layer : _layers.as_array()) {
    std::string layer_id = _layer.at("layer_id").get_string().data();
    auto layer = layers[layer_id];
    if (layer) {
      try {
        time_t update_time = atoll(_layer.at("time").as_string().data());
        if (layer->id == active_layer_id) {
          auto now = DateTime::now();
#if 1  // aug
          if (update_time > layer->last_update) {
#else
          if (test_time >= layer->last_update) {
#endif
            layer->last_update = update_time;  // now;
            time_t forecast_time = ((now / 1800) + 1) * 1800;
            GetImageAt(*layer, forecast_time, forecast_time
                + forecast_count * 1800, update_time, Skysight::DownloadComplete);
            }
        }  else {
          if (layer) // not the active layer
            layer->last_update = update_time;  // now;
        }
      }
      catch (std::exception &e) {
        LogError(std::current_exception(), e.what());
      }
    }
  }

  if (!layers_vector.empty()) {
    success = true;
    inited_lastupdates = success;
    int i = 0;
    std::string str;
    for (auto &layer : layers) if (layer.second) {
      str += DateTime::time_str(layer.second->last_update, "%H:%M:%S") + ", ";
      if (++i > 3)
        break;
    }

    lastupdates_time = DateTime::now();
    LogFmt("SkySight::LastUpdated count = {}: {}... ",
      layers.size(), str);
    last_request = 0;  // the next request can be started
    TimerInvoke();
  }
  return success;
}

bool
SkysightAPI::UpdateDatafiles(const boost::json::value &_datafiles)
{
  bool success = false;
  auto active_layer = Skysight::GetActiveLayer();
  if (active_layer) {
    std::string_view active_layer_id = active_layer->id;

    for (auto &_filepoint : _datafiles.as_array()) {
      std::string layer_id = _filepoint.at("layer_id").get_string().data();
      time_t time_index = _filepoint.at("time").to_number<time_t>();
      std::string link = _filepoint.at("link").get_string().data();

      if ((time_index > 0) && !link.empty() && layer_id == active_layer_id ) {
        auto layer = GetLayer(layer_id);
        if (layer && layer->last_update == 0)
          layer->last_update = time_index;
        success = GetData(SkysightCallType::Data, layer_id, time_index,
          time_index + 2 * ONE_HOUR, link);

        if (!success)
          return false;
      }
    }
  }
  return success;
}

bool
SkysightAPI::ParseDataDetails(const SkysightRequestArgs &args,
  const boost::property_tree::ptree &details)
{
  bool success = false;
  time_t time_index;

  for (auto &j : details) {
    auto time = j.second.find("time");
    auto link = j.second.find("link");
    if ((time != j.second.not_found()) && (link != j.second.not_found())) {
      time_index = static_cast<time_t>(std::strtoull(
        time->second.data().c_str(), NULL, 0));

      if (time_index > (time_t)args.to) {
        if (!success)
          MakeCallback(args.cb, "", false, args.layer.c_str(), args.from);
        return success;
      }

      auto layer = GetLayer(args.layer);
      if (layer && layer->last_update == 0)
        layer->last_update = time_index;
      success = GetData(SkysightCallType::Data, args.layer.c_str(), time_index,
        args.to, link->second.data().c_str(), args.cb);

      if (!success)
        return false;
    }
  }

  return success;
}
bool
SkysightAPI::ParseDataDetails(const SkysightRequestArgs &args,
                              const std::string &result)
{
  boost::property_tree::ptree details;
  if (!GetResult(args, result.c_str(), details)) {
    MakeCallback(args.cb, "", false, args.layer.c_str(), args.from);
    return false;
  }
  return ParseDataDetails(args, details);
}


#ifdef SKYSIGHT_FORECAST
void
SkysightAPI::CallCDFDecoder(const SkySight::Layer *layer, const time_t _time, 
  const std::string_view &cdf_file,
  const std::string_view& output_img, const SkysightCallback _callback)
{
  queue.AddDecodeJob(std::make_unique<CDFDecoder>(
    cdf_file.data(), output_img.data(), layer->id.c_str(), _time,
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
  constexpr uint16_t o = 1;

  for (tile.x = base_tile.x - o; tile.x <= base_tile.x + o; tile.x++)
    for (tile.y = base_tile.y - o; tile.y <= base_tile.y + o; tile.y++) {

      if (!GeoBitmap::GetBounds(tile).Overlaps(map_bounds))
          continue;  // w/o overlapping no Request!
      const std::string url = GetUrl(type, layer_id, from, tile);
      const auto path = GetPath(type, layer_id, from, tile);

      if (File::Exists(path)) {
        MakeCallback(cb, path.c_str(), true, layer_id.data(), from);
      }  else {
        if (co_request)
          co_request->DownloadFile(url, path, SKYSIGHT_LIVE_REQUEST);
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
  const 
    std::string url = link.empty() ? GetUrl(type, layer_id, from) : std::string(link);
  const auto path = GetPath(type, layer_id, from);

  switch (type) {
    case SkysightCallType::Tile:
      assert(false);
      return false;
    case SkysightCallType::Data:
      if (File::Exists(path)) {
        Skysight::GetSkysight()->SetUpdateFlag();
        MakeCallback(cb, path.c_str(), true, layer_id.data(), from);
        return true;  // don't create request if file exists
      } else {
        // This only gets the zip or nc file, does not yet do the conversion
        if (co_request)
          co_request->DownloadFile(url, path, SKYSIGHT_FORECAST_REQUEST);
        return true;
      }
      break;
    case SkysightCallType::DataDetails: {
      std::stringstream url_part;
      url_part << "data?region_id=" << region << "&layer_ids=" << layer_id;
      url_part << "&from_time=" << from;

      if (co_request)
        co_request->RequestJson("datafiles", url_part.str());
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
    layer_updated = GetLayer(layer)->last_update;
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

bool
SkysightAPI::GetImageAt(SkySight::Layer &layer,
  time_t fctime,
  time_t maxtime,
  [[maybe_unused]] time_t update_time,
  SkysightCallback cb)
{
  return GetData(SkysightCallType::DataDetails, layer.id.c_str(),
    fctime, maxtime, cb);
}

bool
SkysightAPI::GetImageAt(const char *const layer, time_t fc_time,
  time_t maxtime, SkysightCallback cb)
{
  auto max_index = maxtime;
  auto search_index = fc_time;

  bool found_image = true;
  while (found_image && (search_index <= max_index))
  {
    auto path = GetPath(SkysightCallType::Image, layer, search_index);
    found_image = CacheAvailable(path, SkysightCallType::Image, layer);

    if (found_image)
    {
      search_index += HALF_HOUR;

      if (search_index > max_index)
      {
        MakeCallback(cb, path.c_str(), true, layer, fc_time);
        return true;
      }
    }
  }

  return GetData(SkysightCallType::DataDetails, layer, fc_time, max_index,
                 cb);
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

void
SkysightAPI::OnTimer()
{
  const std::lock_guard lock{ mutex };

  // various maintenance actions
  auto now = DateTime::now();

#ifdef THREAD_TIMER_START
  timer_start(the_function_to_delay, 1000);
#endif  // THREAD_TIMER_START

  if (!co_request)
    return;  // do nothing

  if (!IsLoggedIn()) {
    last_request = now;
    co_request->RequestCredentialKey();
    return;
  }

  if (!inited_regions) {
    if (now < last_request + 5)
      return;
    last_request = now;
    co_request->RequestJson("regions", "regions");
    return;
  } else if (!inited_layers) {
    if (now < last_request + 5)
      return;
    last_request = now;
    co_request->RequestJson("layers","layers?region_id=" + region);
    return;
  } else if (!inited_lastupdates || (now > lastupdates_time + 5 * ONE_MINUTE)) {
    if (now < last_request + 5)
      return;
    last_request = now;
    co_request->RequestJson("last_updated","data/last_updated?region_id=" + region);
    return;
  } else if (!IsInited()) {
    if (now < last_request + 5)
      return;
    last_request = now;
  }

  auto active_layer = Skysight::GetActiveLayer();
  if (active_layer) {
    if (active_layer->tile_layer) {
      if (IsLoggedIn()) {
        GetTileData(active_layer->id.c_str(), now, now,
          Skysight::DownloadComplete);
      }
    } else {
      // TODO(aug): every 5 or 10 minutes only 
      if (active_layer) {
        LogFmt("New Forecast request for {}!", active_layer->id);
        GetData(SkysightCallType::DataDetails, active_layer->id.c_str(), now, now,
          Skysight::DownloadComplete);
      }
    }
  }
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
  for (auto &layer : selected_layers)
    if (layer.id == id)
      return true;

  return false;
}

bool
SkysightAPI::SelectedLayersFull()
{
  return (selected_layers.size() >= SKYSIGHT_MAX_LAYERS);
}
