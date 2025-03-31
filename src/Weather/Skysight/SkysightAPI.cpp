// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Weather/Skysight/SkysightAPI.hpp"
#include "Weather/Skysight/Skysight.hpp"
#include "Weather/Skysight/Request.hpp"
#include "Weather/Skysight/SkySightCoRequest.hpp"
#include "Weather/Skysight/SkysightRegions.hpp"
#include "Weather/Skysight/Layers.hpp"

#include "co/Task.hxx"
#include "Weather/Skysight/SkySightCoRequest.hpp"
#include "Operation/PluggableOperationEnvironment.hpp"
#include "net/http/Init.hpp"

#include "util/StaticString.hxx"

#include "MapWindow/GlueMapWindow.hpp"
#include "UIGlobals.hpp"

#include "LocalPath.hpp"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"

#include "Operation/Operation.hpp"
#include "io/BufferedReader.hxx"
#include "io/FileLineReader.hpp"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Request.hxx"
#include "time/DateTime.hpp"
#include "io/ZipArchive.hpp"
#include "ui/canvas/custom/GeoBitmap.hpp"
#include "Geo/GeoBounds.hpp"
#include "time/BrokenDateTime.hpp"
#include "time/DateTime.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <windef.h>  // for MAX_PATH

SkysightAPI *SkysightAPI::self;

#ifdef _DEBUG
static constexpr uint32_t forecast_count = 2;
#else
static constexpr uint32_t forecast_count = 6;
#endif

SkysightAPI::~SkysightAPI()
{
  LogFormat("SkysightAPI::~SkysightAPI %d", timer.IsActive());
  timer.Cancel();
}

SkysightLayer *
SkysightAPI::GetLayer(size_t index)
{
  assert(index < layers_vector.size());
  auto &layer = layers_vector.at(index);

  return &layer;
}

SkysightLayer *
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
    case SkysightCallType::Regions:
      url.Format("%s/regions", SKYSIGHTAPI_BASE_URL);
      break;
    case SkysightCallType::Layers:
      url.Format("%s/layers?region_id=%s", SKYSIGHTAPI_BASE_URL,
        region.c_str());
      break;
    case SkysightCallType::LastUpdates:
      url.Format("%s/data/last_updated?region_id=%s", SKYSIGHTAPI_BASE_URL,
        region.c_str());
      break;
    case SkysightCallType::DataDetails:
      url.Format("%s/data?region_id=%s&layer_ids=%s&from_time=%llu", 
        SKYSIGHTAPI_BASE_URL,region.c_str(), layer_id.data(), from);
      break;
    case SkysightCallType::Data:
      // caller should already have an URL
      break;
    case SkysightCallType::Tile: {
#ifdef SKYSIGHT_LIVE
      if (layer_id.starts_with("osm"))
        url.Format("%s/", OSM_BASE_URL);
      else {
        url.Format("%s/%s/", SKYSIGHTAPI_BASE_URL, layer_id.data());
      }
      url.AppendFormat("%u/%u/%u", tile.zoom, tile.x, tile.y);
      // example: https://tile.openstreetmap.org/11/1103/685.png
      if (layer_id.starts_with("osm"))
        url.append(".png");
      else {
        auto last_time = (from / _10MINUTES - 1) * _10MINUTES;
        url.AppendFormat("/%s", DateTime::time_str(last_time, "%Y/%m/%d/%H%M").c_str());
      }
#else
    // caller should already have an URL
#endif
    }
    break;
  case SkysightCallType::Image:
    // no url because no skysight server request!
    break;
  case SkysightCallType::Login:
    url.Format("%s/auth", SKYSIGHTAPI_BASE_URL);
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
    filename = "regions.json";
    break;
  case SkysightCallType::Layers:
    filename.Format("layers-%s.json", region.c_str());
    break;
  case SkysightCallType::LastUpdates:
    filename.Format("lastupdated-%s.json", region.c_str());
    break;
  case SkysightCallType::DataDetails:
    filename.Format("datafiles-%s-%s-%s.json", region.c_str(), layer_id.data(),
       DateTime::time_str(fctime, "%d-%H%M").c_str());
     break;
  case SkysightCallType::Data: {
    auto layer = GetLayer(layer_id);
    filename.Format("%s-%s-%llu-%s.nc", region.c_str(), layer_id.data(),
      layer ? layer->last_update : 0, 
      DateTime::time_str(fctime, "%d-%H%M").c_str());
    }
    break;
  case SkysightCallType::Tile:
    if (1)  // Tile..
  {
    std::stringstream s;
    std::string_view servername;
    bool is_osm = layer_id.starts_with("osm");
    servername = is_osm ? OSM_BASE_URL : SKYSIGHTAPI_BASE_URL;
    auto path = GetUrl(type, layer_id, fctime, tile).substr(servername.length()+1);
    // substitute '/' with '-':
    for (auto &ch : path)
      if (ch == '/') ch = '-';

    if (is_osm)
      filename = path;  // has already extension .png
    else 
      filename = path + (GetLayer(layer_id)->live_layer ? ".jpg" : ".nc");
    } else {
      filename.Format("%s-%s-%s-%s.nc",
        region.c_str(),
        layer_id.data(),
        DateTime::time_str(GetLayer(layer_id)->last_update, "%d-%H%M").c_str(),
        DateTime::time_str(fctime, "%d-%H%M").c_str()
      );
    }
    break;
  case SkysightCallType::Image:
    if (GetLayer(layer_id)->tile_layer)
      return GetPath(SkysightCallType::Tile, layer_id, fctime).WithSuffix(".jpg");
    else 
      return GetPath(SkysightCallType::Data, layer_id, fctime).WithSuffix(".tif");
  case SkysightCallType::Login:
    // local path should not be used
    filename = "credentials.json";
    break;
  }
  return AllocatedPath::Build(cache_path, filename);
}

SkysightAPI::SkysightAPI(std::string_view email, std::string_view password,
                         std::string_view _region, SkysightCallback cb)
    : cache_path(MakeLocalPath("skysight"))
{
  self = this;
  inited_regions = false;
  inited_layers = false;
  inited_lastupdates = false;
  LoadDefaultRegions();

  region = (_region.empty()) ? "EUROPE" : _region;
  if (regions.find(region) == regions.end()) {
    region = "EUROPE";
  }


  if (email.empty() || password.empty())
    // w/o eMail and password no communication possible
    return;

  queue.SetCredentials(email, password);

//  GetData(SkysightCallType::Login, cb);
  GetData(SkysightCallType::Regions, cb);

  if (timer.IsActive()) {
    timer.Cancel();
  }
  // Check for maintenance actions every 5 mins
  // timer.Schedule(std::chrono::minutes(5));
  // TODO(August2111): now for test - 1 Minute:
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
      self->queue.Clear("Login error");
    } else {
      self->MakeCallback(req.cb, result.c_str() , false, req.layer.c_str(),
			 req.from);
    }
    return;
  }

  switch (req.calltype) {
  case SkysightCallType::Regions:
    self->ParseRegions(req, result);
    break;
  case SkysightCallType::Layers:
    self->ParseLayers(req, result);
    break;
  case SkysightCallType::LastUpdates:
    self->ParseLastUpdates(req, result);
    break;
  case SkysightCallType::DataDetails:
    self->ParseDataDetails(req, result);
    break;
  case SkysightCallType::Data:
    self->ParseData(req, result);
    break;
  case SkysightCallType::Tile:
    self->ParseTile(req, result);
    break;
  case SkysightCallType::Image:
    break;
  case SkysightCallType::Login:
    self->ParseLogin(req, result);
  default:
    break;
  }
}

bool
SkysightAPI::ParseRegions(const SkysightRequestArgs &args,
                          const std::string &result)
{
  boost::property_tree::ptree details;
  
  if (!GetResult(args, result.c_str(), details)) {
    LoadDefaultRegions();
    return false;
  }

  regions.clear();

  bool success = false;
  
  for (auto &i: details) {
    boost::property_tree::ptree &node = i.second;
    auto id = node.find("id");
    auto name = node.find("name");
    if (id != node.not_found()) {
      regions.insert(std::pair<std::string, std::string>(id->second.data(),
                     name->second.data()));
      success = true;
    }
  }

  if (success) {
    inited_regions = true;
  } else { //fall back to defaults
    LoadDefaultRegions();
    return false;
  }

  /* region loaded from settings is not in our regions list.Fall back to
   * Europe. */
  if (regions.find(region) == regions.end()) {
    region = "EUROPE";
    return false;
  }

  if (success) {
    if (!inited_layers)
      GetData(SkysightCallType::Layers, args.cb);
    else
      MakeCallback(args.cb, result.c_str(), success, "", 0);

    inited_regions = true;
    return true;
  }

  MakeCallback(args.cb, "", success, "", 0);
  return false;
}

void
SkysightAPI::LoadDefaultRegions()
{
  for (auto r = skysight_region_defaults; r->id != nullptr; ++r)
    regions.emplace(std::pair<std::string, std::string>(r->id, r->name));
}

bool
SkysightAPI::ParseLayers(const SkysightRequestArgs &args,
                         const std::string &result)
{
  boost::property_tree::ptree details;

  if (!GetResult(args, result.c_str(), details)) {
    MakeCallback(args.cb, "", false, "", 0);
    return false;
  }

  layers_vector.clear();
  bool success = false;

#ifdef SKYSIGHT_LIVE
  layers_vector.push_back({ "satellite","Satellite", "live satellite images" });
  layers_vector.push_back({ "rain","Rain", "live rain layer" });
  layers_vector.push_back({ "osm","Open Street Map", "OSM layer" });
  layers_vector[0].live_layer = layers_vector[0].tile_layer = true;
  layers_vector[1].live_layer = layers_vector[0].tile_layer = true;
  layers_vector[0].tile_layer = true;
#endif

#ifdef SKYSIGHT_FORECAST 
  for (auto &i: details) {
    boost::property_tree::ptree &node = i.second;
    auto id = node.find("id");
    auto legend = node.find("legend");
    if (id != node.not_found() && legend != node.not_found()) {
      SkysightLayer m = SkysightLayer(
        std::string(id->second.data()), 
        std::string(node.find("name")->second.data()),
        std::string(node.find("description")->second.data())
      );

      auto colours = legend->second.find("colors");
      if (colours != legend->second.not_found()) {
        success = true;
        for (auto &j : colours->second) {
          auto c = j.second.get_child("color").begin();
          m.legend.insert(std::pair<float, LegendColor>(
            std::stof(j.second.find("value")->second.data()),
            {
              static_cast<uint8_t>(std::stoi(c->second.data())),
              static_cast<uint8_t>(std::stoi(std::next(c, 1)->second.data())),
              static_cast<uint8_t>(std::stoi(std::next(c, 2)->second.data()))
            }
          ));
        }
        layers_vector.push_back(m);
        // VORSICHT: layers[m.id] = &m;
      }
    }
  }
#endif  // SKYSIGHT_FORECAST 

#ifdef SKYSIGHT_LIVE
  for (auto &layer : layers_vector) {
    if (layer.id == "satellite") {
      layer.tile_layer = true;
      layer.live_layer = true;
      layer.zoom_max = 8;
    } else if (layer.id == "rain") {
      layer.tile_layer = true;
      layer.live_layer = true;
      layer.zoom_max = 8;
    } else if (layer.id == "osm") {
      layer.tile_layer = true;
    }
  }
#endif  // SKYSIGHT_LIVE

  if (success) {
    if (!inited_lastupdates)
      GetData(SkysightCallType::LastUpdates, args.cb);
    else
      MakeCallback(args.cb, result.c_str(), success, "", 0);

    inited_layers = true;
    return true;
  }

  MakeCallback(args.cb, "", false, "", 0);

  return false;
}

bool
SkysightAPI::ParseLastUpdates(const SkysightRequestArgs &args,
                              const std::string &result)
{
  bool success = false;
  boost::property_tree::ptree details;
  if (!GetResult(args, result.c_str(), details)) {
    MakeCallback(args.cb, "", false, "", 0);
    return false;
  }

  auto active_layer = Skysight::GetActiveLayer();
  std::string_view layer_id;
  if (active_layer) {
    layer_id = active_layer->id;
    if (active_layer->tile_layer) {
      inited_lastupdates = success;
    } else for (auto &layer : layers_vector) {
      for (auto &j : details)
      {
        /* std::string -  not std::string_view because after j.second.get the
        * data will be empty! */
        std::string tst = j.first.data(); 
        std::string msg = j.second.data(); 
        if (!tst.empty() && (tst == "message") &&
             !msg.empty() && (msg == "Bad API Key")) {
          queue.SetKey("", 0);
          return false;
        }
        std::string id = j.second.get("layer_id", "");
        if (id.empty())
          continue;
        time_t time = j.second.get("time", 0);
        if (!time)
          continue;

        if (layer.id == id) {
          auto update_time = time;
          if ((layer_id == layer.id))
          {
#if 0  // aug
            if (update_time > layer.last_update) {
#else
            if (update_time >= layer.last_update) {
#endif
              layer.last_update = update_time;
              time_t forecast_time = ((std::time(0) / 1800) + 1) * 1800;
              GetImageAt(layer, forecast_time, forecast_time
                + forecast_count * 1800, update_time, Skysight::DownloadComplete);
              success = true;
            } else if (update_time == layer.last_update) { // no changes
              success =  ParseDataDetails(args, "");
            }
          }
        }
      }
    }
  } else {
    // layer_id = Profile::Get(ProfileKeys::WeatherLayerDisplayed);
    layer_id = "n.a.";
    ///  success = true;
  }

  inited_lastupdates = success;
  lastupdates_time = success ? DateTime::now() : 0;
  MakeCallback(args.cb, "", success, "", 0);

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

      // const auto path = GetPath(SkysightCallType::Data, args.layer.data(), time_index);
      // if (File::Exists(path)) {
      //   MakeCallback(args.cb, path.c_str(), true, args.layer.data(), time_index);
      // }
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

bool
SkysightAPI::ParseLogin(const SkysightRequestArgs &args,
                        const std::string &result)
{
  boost::property_tree::ptree details;
  if (!GetResult(args, result.c_str(), details)) {
    queue.Clear("Login error");
    return false;
  }

  bool success = false;
  auto key = details.find("key");
  auto valid_until = details.find("valid_until");

  if ((key != details.not_found()) && (valid_until != details.not_found())) {
    queue.SetKey(key->second.data().c_str(),
                 static_cast<time_t>(std::strtoull(
                     valid_until->second.data().c_str(), NULL, 0)));
    success = true;
    LogFormat("SkysightAPI::ParseLogin success with key %s",
              key->second.data().c_str());

    // auto _key = key->second.data().c_str();
    std::string _key = key->second.data().c_str();
    if (co_request == nullptr)
      co_request = new SkysightCoRequest(_key);  // _key);
    else
      co_request->SetCredentialKey(_key);  // key->second.data().c_str(), "", "");
      // co_request->RequestCredentialKey("", "");  // key->second.data().c_str(), "", "");

    // TODO: trim available regions from allowed_regions
  } else {
    queue.Clear("Login error");
    LogFormat("SkysightAPI::ParseLogin failed");
  }
  return success;
}

#ifdef SKYSIGHT_FORECAST 
void
SkysightAPI::CallCDFDecoder(const SkysightRequestArgs &args,
  [[maybe_unused]] const std::string_view& output_img)
{
  queue.AddDecodeJob(std::make_unique<CDFDecoder>(
    args.path.c_str(), output_img.data(), args.layer.c_str(), args.from,
    GetLayer(args.layer)->legend, args.cb));
}
#endif  // SKYSIGHT_FORECAST 

bool
SkysightAPI::ParseData(const SkysightRequestArgs &args,
                       [[maybe_unused]] const std::string &result)
{
  std::string output_img =
      GetPath(SkysightCallType::Image, args.layer.c_str(), args.from).c_str();
//  std::string s = output_img.c_str();
//  s = GetPath(SkysightCallType::Image, args.layer.c_str(), args.from).c_str();
  char buffer[16];  // a test buffer at beginning of file
  auto filepath = Path(args.path.c_str());
  File::ReadString(filepath, buffer, sizeof(buffer));
  if (strncmp(buffer, "<?xml version=", 14) == 0) {
    // this is an (error) message, no zip file or CDF-File
    LogFmt("XML-File: {}", buffer);
#ifdef SKYSIGHT_FORECAST 
  } else if (strncmp(buffer, "CDF", 3) == 0) {
    // only the CDF file has to be decoded
    if (result.ends_with(".nc")) {
      CallCDFDecoder(args, output_img);
    } // else {}
#endif   // SKYSIGHT_FORECAST 
  }  else if (args.path.ends_with(".jpg")) {
    if (args.cb)
      args.cb(result, true, Skysight::GetActiveLayer()->id, 0);
  }  else { 
    auto zip_file = filepath.WithSuffix(".zip");
    File::Rename(filepath, zip_file);
    ZipIO::UnzipSingleFile(zip_file, filepath);  // use the same name for unzipped file
    File::ReadString(filepath, buffer, sizeof(buffer));  // re-read again
#ifdef SKYSIGHT_FORECAST 
    if (strncmp(buffer, "CDF", 3) == 0) {
      // and now it is a CDF file
      CallCDFDecoder(args, output_img);  // result); // 
    }
#endif  // SKYSIGHT_FORECAST 
  }
   return true;
}

bool
SkysightAPI::ParseTile(const SkysightRequestArgs &args,
                       [[maybe_unused]] const std::string &result)
{
  std::string output_img =
      GetPath(SkysightCallType::Image, args.layer.c_str(), args.from).c_str();
  if (args.path.ends_with(".jpg")) {
    if (args.cb)
      args.cb(result, true, Skysight::GetActiveLayer()->id, 0);
  }
  return true;
}

bool
SkysightAPI::GetTileData(const std::string_view layer_id,
    const time_t from, const time_t to,
    SkysightCallback cb, bool force_recache)
{

  GlueMapWindow *map_window = UIGlobals::GetMap();
  GeoBitmap::TileData base_tile;
  auto layer = GetLayer(layer_id);
  const SkysightCallType type = SkysightCallType::Tile;
  if (map_window) { // && map_window->IsPanning()) {
    base_tile = GeoBitmap::GetTile(map_window->VisibleProjection(), 
      layer->zoom_min, layer->zoom_max);
  }
  else {
    return false;
  }

  auto tile = base_tile;
  auto map_bounds = map_window->VisibleProjection().GetScreenBounds();

  for (tile.x = base_tile.x - 1; tile.x <= base_tile.x + 1; tile.x++)
    for (tile.y = base_tile.y - 1; tile.y <= base_tile.y + 1; tile.y++) {

      if (!GeoBitmap::GetBounds(tile).Overlaps(map_bounds))
          continue;  // w/o overlapping no Request!
      const std::string url = GetUrl(type, layer_id, from, tile);
      const auto path = GetPath(type, layer_id, from, tile);

      if (File::Exists(path)) {
        MakeCallback(cb, path.c_str(), true, layer_id.data(), from);
        continue;
      }
/**/
      else {
        // static int counter = 0;
        // if (counter++ < 1) {
          // PluggableOperationEnvironment env;
        auto x = co_request->DownloadImage(url, path);  // , "");  //  , *Net::curl, env);
          if (x)
            continue;
          // }
          else
            break;
        // continue;
      }
/* */

      SkysightRequestArgs ra(
        url.c_str(),
        path.c_str(),
        type,
        region.c_str(),
        layer_id.data(),
        from,
        to,
        cb
      );

      /*
        If cache is available, parse it directly regardless of whether async or
      sync
      */
      if (!force_recache && CacheAvailable(path, type)) {
        ParseResponse(path.c_str(), true, ra);
        return true;
      }

      queue.AddRequest(std::make_unique<SkysightAsyncRequest>(ra),
        (type != SkysightCallType::Login));
    }
  return true;
}

bool
SkysightAPI::GetData(SkysightCallType type, const std::string_view layer_id,
    const time_t from, const time_t to,
    [[maybe_unused]] const std::string_view link,
    SkysightCallback cb, bool force_recache)
{
  const std::string url = link.empty() ? GetUrl(type, layer_id, from) : std::string(link);
  const auto path = GetPath(type, layer_id, from);

  switch (type) {
#ifdef SKYSIGHT_DEBUG  // log the path in opensoar.log
    case SkysightCallType::DataDetails:
    case SkysightCallType::Login:
    case SkysightCallType::LastUpdates:
      if (!path.empty())
        LogString(path.c_str());
      break;
#endif
    case SkysightCallType::Tile:
    case SkysightCallType::Data:
        // ParseData(arg, path);
        // const auto path = GetPath(SkysightCallType::Data, args.layer.data(), time_index);
      if (File::Exists(path)) {
        MakeCallback(cb, path.c_str(), true, layer_id.data(), from);
        return true;  // don't create request if file exists
      } else {
        // PluggableOperationEnvironment env;
        // SkysightCoRequest::DownloadImage(url, path, ""); // , *Net::curl, env );
        return true;
      }
      break;
    default:
      break;
  }

  SkysightRequestArgs ra(
    url.c_str(),
    path.c_str(),
    type,
    region.c_str(),
    layer_id.data(),
    from,
    to,
    cb
  );

  /*
    If cache is available, parse it directly regardless of whether async or
  sync
  */
  if (!force_recache && CacheAvailable(path, type)) {
    ParseResponse(path.c_str(), true, ra);
    return true;
  }

  queue.AddRequest(std::make_unique<SkysightAsyncRequest>(ra),
                   (type != SkysightCallType::Login));

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
    case SkysightCallType::Regions:
    case SkysightCallType::Layers:
      // cached for as long as we have the files to allow fast startup
      return true;
      break;
    case SkysightCallType::LastUpdates:
      // always retrieve last updates if requested
      return false;
      break;
    case SkysightCallType::Image:
      if (!layer)
	      return false;
      return (layer_updated <= File::GetTime(path));
      break;
    case SkysightCallType::DataDetails:
    case SkysightCallType::Data:
    case SkysightCallType::Login:
      // these aren't cached to disk
    case SkysightCallType::Tile:
      // these aren't cached to disk ???
      return false;
      break;
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
SkysightAPI::GetImageAt(SkysightLayer &layer,
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
      search_index += (60 * 30);

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

  self->GetData(SkysightCallType::Login);
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
  // various maintenance actions
  auto now = std::time(0);

  // refresh regions cache file if > 24h old
  auto p = GetPath(SkysightCallType::Regions);
  if (File::Exists(p) && (File::GetTime(p) + 24*60*60 < now))
    GetData(SkysightCallType::Regions, nullptr, true);

  // refresh layers cache file if > 24h old
  p = GetPath(SkysightCallType::Layers);
  if (File::Exists(p) && (File::GetTime(p) + 24 * 60 * 60 < now))
    GetData(SkysightCallType::Layers, nullptr, true);


  auto active_layer = Skysight::GetActiveLayer();
  if (active_layer) {
    if (active_layer->tile_layer) {
      // GetData(SkysightCallType::Tile, active_layer->id.c_str(), now,
      //   now, Skysight::DownloadComplete);
      GetTileData(active_layer->id.c_str(), now,
        now, Skysight::DownloadComplete);
    } else {
      /* the timer is called every minute, LastUpdate should only check after
      5 minutes to prevent too many server accesses */
      if (!inited_lastupdates || (std::time(0) > lastupdates_time + 5*60))
      // if (std::time(0) > lastupdates_time + 5*60)
        GetData(SkysightCallType::LastUpdates);
    }
  }
}

/*
 * ******   PRESELECTED LEYERS ************
 *
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

