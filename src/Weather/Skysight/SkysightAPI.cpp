// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Weather/Skysight/SkysightAPI.hpp"
#include "Weather/Skysight/Skysight.hpp"
#include "Weather/Skysight/Request.hpp"
#include "Weather/Skysight/SkysightRegions.hpp"
#include "Weather/Skysight/Layers.hpp"

#include "util/StaticString.hxx"

#include "LocalPath.hpp"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"

#include "Operation/Operation.hpp"
#include "io/BufferedReader.hxx"
#include "io/FileLineReader.hpp"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Request.hxx"
#include "time/BrokenDateTime.hpp"
#include "ZipArchive.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <windef.h> /* for MAX_PATH */

#ifdef USE_STD_FORMAT
# include <format>
#endif // USE_STD_FORMAT
SkysightAPI *SkysightAPI::self;

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
  return (std::find(layers_vector.begin(), layers_vector.end(), id) != layers_vector.end());
}

int
SkysightAPI::NumLayers()
{
  return (int)layers_vector.size();
}

const std::string
SkysightAPI::GetUrl(SkysightCallType type, const std::string_view layer_id,
                    const time_t from)
{
  StaticString<256> url;
  switch (type) {
  case SkysightCallType::Regions:
    url = SKYSIGHTAPI_BASE_URL "/regions";
    break;
  case SkysightCallType::Layers:
    url.Format(SKYSIGHTAPI_BASE_URL "/layers?region_id=%s", region.c_str());
    break;
  case SkysightCallType::LastUpdates:
    url.Format(SKYSIGHTAPI_BASE_URL "/data/last_updated?region_id=%s",
               region.c_str());
    break;
  case SkysightCallType::DataDetails:
    url.Format(SKYSIGHTAPI_BASE_URL "/data?region_id=%s&layer_ids=%s"
      "&from_time=%llu", region.c_str(), layer_id.data(), from);
    break;
  case SkysightCallType::Data:
  case SkysightCallType::Image:
    // caller should already have URL
    break;
  case SkysightCallType::Login:
    url = SKYSIGHTAPI_BASE_URL "/auth";
    break;
  }
  return url.c_str();
}

AllocatedPath
SkysightAPI::GetPath(SkysightCallType type, const std::string_view layer_id,
                     const time_t fctime)
{
  StaticString<256> filename;
  BrokenDateTime fc;
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
    fc = BrokenDateTime::FromUnixTime(fctime);
    filename.Format("%s-datafiles-%s-%02d-%02d%02d.json",
                    region.c_str(), layer_id.data(),
                    fc.day, fc.hour, fc.minute);
    break;
  case SkysightCallType::Data:
    {
      auto layer = GetLayer(layer_id);
      auto update_time =
          std::chrono::system_clock::from_time_t(layer->last_update);
      auto prop_time =
          std::chrono::system_clock::from_time_t(fctime);
#ifdef USE_STD_FORMAT
      filename.Format(
        "%s-%s-%s_%s.nc", region.c_str(), layer,
        std::format("{:%d-%H%M}", 
          floor<std::chrono::minutes>(update_time)).c_str(),
        std::format("{:%d-%H%M}",
          floor<std::chrono::minutes>(prop_time)).c_str());
#else  // USE_STD_FORMAT
          std::stringstream s;
          auto tx1 = std::chrono::system_clock::to_time_t(update_time);
          auto tx2 = std::chrono::system_clock::to_time_t(prop_time);
          s << region << '-' << layer_id << '-'
            << std::put_time(std::localtime(&tx1), "%d-%H%M") << '-'
            << std::put_time(std::localtime(&tx2), "%d-%H%M") << ".nc";
          filename.SetASCII(s.str().c_str());
#endif // USE_STD_FORMAT

    }
    break;
  case SkysightCallType::Image:
    return GetPath(SkysightCallType::Data, layer_id, fctime).WithSuffix(".tif");
    break;
  case SkysightCallType::Login:
    // local path should not be used
    filename = "credentials.json";
    break;
  }
  return AllocatedPath::Build(cache_path, filename);
}

SkysightAPI::SkysightAPI(std::string email, std::string password,
                         std::string _region, SkysightCallback cb)
    : cache_path(MakeLocalPath("skysight"))
{
  self = this;
  inited_regions = false;
  LoadDefaultRegions();

  region = (_region.empty()) ? "EUROPE" : _region;
  if (regions.find(region) == regions.end()) {
    region = "EUROPE";
  }

  inited_layers = false;
  inited_lastupdates = false;

  if (email.empty() || password.empty())
    return;

  queue.SetCredentials(email.c_str(), password.c_str());

//  GetData(SkysightCallType::Login, cb);
  GetData(SkysightCallType::Regions, cb);

  if (timer.IsActive()) {
    timer.Cancel();
  }
  // Check for maintenance actions every 5 mins
  timer.Schedule(std::chrono::minutes(5));
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

    for (auto &layer : layers_vector) {
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
        uint64_t time = j.second.get("time", 0);
        if (!time)
          continue;

        if (layer.id == id) {
          auto update_time = time;
          // std::strtoull(time.data(), NULL, 0);
          if ((layer_id == layer.id))
          {
#if 1  // aug
            if (update_time > layer.last_update) {
#else
            if (update_time >= layer.last_update) {
#endif
              layer.last_update = update_time;
              BrokenDateTime time = BrokenDateTime::NowUTC();
#ifdef _DEBUG
              GetImageAt(layer, time, time + std::chrono::hours(1),  // = 2 tiff images
#else
              GetImageAt(layer, time, time + std::chrono::hours(3),  // (5), = 6 tiff images
#endif
                update_time, Skysight::DownloadComplete);
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
  }


  inited_lastupdates = success;
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

    // TODO: trim available regions from allowed_regions
  } else {
    queue.Clear("Login error");
    LogFormat("SkysightAPI::ParseLogin failed");
  }
  return success;
}

bool
SkysightAPI::ParseData(const SkysightRequestArgs &args,
                       [[maybe_unused]] const std::string &result)
{
  auto output_img =
      GetPath(SkysightCallType::Image, args.layer.c_str(), args.from);
  char buffer[16];  // a test buffer at beginning of file
  auto filepath = Path(args.path.c_str());
  File::ReadString(filepath, buffer, sizeof(buffer));
  if (strncmp(buffer, "<?xml version=", 14) == 0) {
    // this is an (error) message, no zip file or CDF-File
    LogFmt("XML-File: {}", buffer);
  } else if (strncmp(buffer, "CDF", 3)) {
      ZipIO::UnzipSingleFile(filepath, filepath);  // use the same name for unzipped file
  }
  queue.AddDecodeJob(std::make_unique<CDFDecoder>(
      args.path.c_str(), output_img.c_str(), args.layer.c_str(), args.from,
      GetLayer(args.layer)->legend, args.cb));
   return true;
}

bool
SkysightAPI::GetData(SkysightCallType t, const std::string_view layer_id,
                     const time_t from, const time_t to,
                     const std::string_view link, SkysightCallback cb,
                     bool force_recache)
{
  const std::string url = link.empty() ? GetUrl(t, layer_id, from) : std::string(link);

  const auto path = GetPath(t, layer_id, from);

#if SKYSIGHT_DEBUG  // log the path in opensoar.log
  if ((t == SkysightCallType::DataDetails ||
      t == SkysightCallType::Login ||
      t == SkysightCallType::LastUpdates) &&
      !path.empty())
    LogString(path.c_str());
#endif

  SkysightRequestArgs ra(
    url.c_str(),
    path.c_str(),
    t,
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
  if (!force_recache && CacheAvailable(path, t)) {
    ParseResponse(path.c_str(), true, ra);
    return true;
  }

  queue.AddRequest(std::make_unique<SkysightAsyncRequest>(ra),
                   (t != SkysightCallType::Login));

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
      return (layer_updated <= (time_t)std::chrono::system_clock::to_time_t(File::GetLastModification(path)));
      break;
    case SkysightCallType::DataDetails:
    case SkysightCallType::Data:
    case SkysightCallType::Login:
      // these aren't cached to disk
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

#if 1 // TODO(August2111): Variant 1!
// SkysightAPI::GetImageAt(1) with layer(!), start_time, max_time, update_time and callback
bool
SkysightAPI::GetImageAt(SkysightLayer &layer,
  BrokenDateTime fctime,
  BrokenDateTime maxtime,
  [[maybe_unused]] uint64_t update_time,
  SkysightCallback cb)
{
#if 0
  // round time to nearest 30-min forecast slot
  constexpr uint64_t forecast_diff = 60 * 30; // 30 min in sec
  fctime.second = 0;
  //  fctime = fctime + std::chrono::seconds(forecast_diff);
  if (fctime.minute < 15) {
    fctime.minute = 30;
  } else {
    fctime = fctime + std::chrono::hours(1); //  seconds(60 * 60);
    fctime.minute = (fctime.minute < 45) ? 0 :30;
  }
//  return true;
  //auto time_index = std::chrono::system_clock::to_time_t(fctime.ToTimePoint());
#endif
  return GetData(SkysightCallType::DataDetails, layer.id.c_str(),
    std::chrono::system_clock::to_time_t(fctime.ToTimePoint()),
    std::chrono::system_clock::to_time_t(maxtime.ToTimePoint()), cb);
}
#endif

bool
SkysightAPI::GetImageAt(const char *const layer, BrokenDateTime fctime,
                        BrokenDateTime maxtime, SkysightCallback cb)
{
  // round time to nearest 30-min forecast slot
  if ((fctime.minute >= 15) && (fctime.minute < 45)) fctime.minute = 30;
  else if (fctime.minute >= 45)
  {
    fctime.minute = 0;
    fctime = fctime + std::chrono::hours(1);
  }
  else if (fctime.minute < 15) fctime.minute = 0;

  auto time_index = std::chrono::system_clock::to_time_t(fctime.ToTimePoint());
  auto max_index = std::chrono::system_clock::to_time_t(maxtime.ToTimePoint());
  auto search_index = time_index;

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
        MakeCallback(cb, path.c_str(), true, layer, time_index);
        return true;
      }
    }
  }

  return GetData(SkysightCallType::DataDetails, layer, time_index, max_index,
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
  auto now = std::chrono::system_clock::to_time_t(
      BrokenDateTime::NowUTC().ToTimePoint());

  // refresh regions cache file if > 24h old
  auto p = GetPath(SkysightCallType::Regions);
  if (File::Exists(p) &&
      (std::chrono::system_clock::to_time_t(File::GetLastModification(p) +
                                            std::chrono::hours(24)) < now))
    GetData(SkysightCallType::Regions, nullptr, true);

  // refresh layers cache file if > 24h old
  p = GetPath(SkysightCallType::Layers);
  if (File::Exists(p) &&
      (std::chrono::system_clock::to_time_t(File::GetLastModification(p) +
                                            std::chrono::hours(24)) < now))
    GetData(SkysightCallType::Layers, nullptr, true);

    // refresh last update times if > 5h (update freq is usually 5 hours)
  for (auto &m : layers_vector) {
    // if ((m.last_update + (5 * 60 * 60)) < (uint64_t)now) {  // = 5h
    if ((m.last_update + (30 * 60 - 5)) < (uint64_t)now) {  // = 30min!
      GetData(SkysightCallType::LastUpdates);
      break;
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

