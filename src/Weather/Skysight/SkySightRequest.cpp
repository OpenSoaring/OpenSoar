// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project


#include "SkySightRequest.hpp"
#include "Skysight.hpp"
#include "SkysightAPI.hpp"
#include "net/http/CoDownload.hpp"
#include "net/http/DownloadManager.hpp"
#include "net/http/Init.hpp"
#include "Operation/PluggableOperationEnvironment.hpp"
#include "Operation/ProgressListener.hpp"
#include "util/StaticString.hxx"
#include "util/PrintException.hxx"
#include "system/Path.hpp"
#include "system/FileUtil.hpp"
#include "LocalPath.hpp"
#include "Version.hpp"

#include "LogFile.hpp"

#include "thread/SafeList.hxx"
#include "lib/curl/Global.hxx"
#include "util/BindMethod.hxx"
#include "time/DateTime.hpp"

#include <string>
#if 0 // !defined(__APPLE__)  // std::format("{{ }}") not possible with Mac, ...
# include <format>
# include <boost/format.hpp>
#endif

static const boost::json::value json_null;  //boost::json::null{};
static std::map<std::string_view, boost::json::value> json_values;

/**
 * This class tracks a download - and updates a #ProgressDialog (if available).
 */
 /////////////////////////////////////////////////////////////////////////////
class SkysightListener final : Net::DownloadListener {
  SkysightRequest *owner;
  PluggableOperationEnvironment env;

  UI::PeriodicTimer update_timer{ [this] { Net::DownloadManager::Enumerate(*this); } };

// ... without notification:
//  UI::Notify download_complete_notify{ [this] { OnDownloadCompleteNotification(); } };

  std::exception_ptr error;

  bool got_size = false, complete = false, success;

public:
  SkysightListener(SkysightRequest *_owner) : owner(_owner)
  {
    update_timer.Schedule(std::chrono::seconds(1));
    Net::DownloadManager::AddListener(*this);
  }

  ~SkysightListener() {
    Net::DownloadManager::RemoveListener(*this);
  }

  void Rethrow() const {
    if (error)
      std::rethrow_exception(error);
  }

private:
  /* virtual methods from class Net::DownloadListener */
  void OnDownloadAdded(const std::string_view name,
    size_t size, size_t position) noexcept override {
    if (size == (size_t)-1 && position == (size_t)-1)
        complete = false;
    if (!complete) {
      if (name == "authent") {
        // for this short download don't update the progress range
      } else {
        if (!got_size && size < (size_t)-100) {
          got_size = true;
          env.SetProgressRange(uint64_t(size) / 1024u);
        }

        if (got_size)
          env.SetProgressPosition(uint64_t(position) / 1024u);
      }
    }
  }

  void OnDownloadComplete(const std::string_view name) noexcept override {
    boost::json::value _json = json_null;
    if (!json_values[name].is_null()) {
      _json = json_values[name];
      if (name == "authent") {
        success = owner->SetCredentialKey(_json);
      } else if (name == "regions") {
        success = owner->GetAPI()->UpdateRegions(_json);
      } else if (name.starts_with("layers")) {
        success = owner->GetAPI()->UpdateLayers(_json);
      } else if (name.starts_with("last_updated")) {
        success = owner->GetAPI()->UpdateLastUpdates(_json);
      } else if (name.starts_with("datafiles")) {
        success = owner->GetAPI()->UpdateDatafiles(_json);
      } else {
        // owner->GetAPI()->SetUpdateFlag();
        Skysight::GetSkysight()->SetUpdateFlag();
        success = true;
#ifdef _DEBUG
        LogFmt("OnDownloadComplete {} - {}", complete ? "complete" : "not compl", name);
#endif
      }
    } else {
      // File-Download...
      success = true;
    }
    if (!complete)
      complete = true;

    // download_complete_notify.SendNotification();   
    if (!_json.is_null()) {
#if defined(SKYSIGHT_FILE_DEBUG)
      std::string now_str = DateTime::str_now();
      std::stringstream s;
      s << Skysight::GetLocalPath().c_str() << '/' << now_str << " " << name << " .json";
      auto path = AllocatedPath(s.str().c_str());
      auto file = File::CreateExclusive(path);
      if (file) {
        auto json_text = boost::json::serialize(_json);
        if (!File::WriteExisting(path, json_text.c_str()))
          LogFmt("File write not successfully!");
      }
#endif
      // 
      json_values[name] = json_null;  // erasing content
    }
  }

  void OnDownloadError(const std::string_view name,
    std::exception_ptr _error) noexcept override {
    if (!complete) {
      complete = true;
      LogFmt("SkySightRequest error with {}", name);
      if (name == "authent") {
        // make Credential to invalid, if the Request goes fail
        owner->SetCredentialKey(nullptr);
      }
      success = false;
      error = std::move(_error);
      // download_complete_notify.SendNotification();
    }
  }

  void OnDownloadCompleteNotification() noexcept {
    assert(complete);
  }
};
/////////////////////////////////////////////////////////////////////////////

bool
SkysightRequest::SetCredentialKey(const boost::json::value &credentials)
try {
  if (credentials.is_null()) {
    key = "";
    valid_until = 0;
    RequestCredentialKey();
    return false;
  }
  key = credentials.at("key").get_string();
  valid_until = credentials.at("valid_until").to_number<time_t>();

  auto regions = credentials.at("allowed_regions").as_array();
  for (auto region : regions)
    allowed_regions.push_back(region.as_string());

  bool logged_in = IsLoggedIn();
  if (!key.empty() && logged_in && request_headers) {
    request_headers->Clear();
    request_headers->AppendFormat("%s: %s", "X-API-Key", key.data());

  }
  if (logged_in) {
    LogFmt("SkySight is valid until {} ({}) = {}s - {}", DateTime::time_str(valid_until, "%Y-%m-%d %H:%M:%S"),
      valid_until, valid_until - DateTime::now(), key);
    Skysight::GetSkysight()->KeyIsNew();
  } else {
    LogFmt("SkySight logged in error... - {}", DateTime::time_str(valid_until, "%Y-%m-%d %H:%M:%S"));
  }
  return logged_in;
}
catch (...) {
  PrintException(std::current_exception());
  return false;
}

bool
SkysightRequest::RequestJson(const std::string_view name, 
  const std::string_view url_part) noexcept
try {
#ifdef _DEBUG
  LogFmt("RequestJson: {}", name);
#endif
  Net::CurlData *data = new Net::CurlData;
  if (request_headers) {
    data->curl_list = request_headers;
  }
  data->json_value = &json_values[name];
  data->type = Net::JSON;
  std::stringstream url;
  url << SKYSIGHTAPI_BASE_URL;
  if (!url_part.empty())
     url << '/' << url_part;
  Net::DownloadManager::Enqueue(url.str(), name, std::move(data));
  return true;
}
catch (...) {
  PrintException(std::current_exception());
  return false;
}

bool
SkysightRequest::RequestCredentialKey() noexcept
try {
  auto now = DateTime::now();
  if (now < request_age + ONE_MINUTE) {
    // Avoid to much requests...
    LogFmt("Request Credential to fast: now = {} vs age = {} ({}), {}",
      now, request_age, DateTime::time_str(request_age, "%H:%M:%S"),
      now - request_age);
    return false;
  }
  if (now + 120  < valid_until) {
    LogFmt("Credential is valid for at least 2 mins: now = {} vs until = {}",
      now, valid_until, DateTime::time_str(valid_until, "%H:%M:%S"),
      valid_until - now);
    return true;
  }
  Net::CurlData *data = new Net::CurlData;

  if (request_headers) {
    request_headers->Clear();
    request_headers->AppendFormat("%s: %s", "X-API-Key", "OpenSoar");
    request_headers->AppendFormat("%s: %s", "User-Agent", OpenSoar_ProductToken);
    request_headers->AppendFormat("%s: %s", "Content-Type", "application/json");
    data->curl_list = request_headers;
  }
  //=========================================
#if 1  // defined(__APPLE__)  // std::format("{{ }}") not possible with Mac, ...
  char buffer[0xFF];
    std::sprintf(buffer, "{ \"username\":\"%s\",\"password\":\"%s\" }",
    username.data(), password.data());
    data->body = buffer;
#else
  data->body = std::format("{{ \"username\":\"{}\",\"password\":\"{}\" }}",
    username, password);
#endif
  data->json_value = &json_values["authent"];
  data->type = Net::JSON;

  request_age = DateTime::now();
  Net::DownloadManager::Enqueue(
    "https://skysight.io/api/auth", "authent", std::move(data));
  return true;
}
catch (...) {
  PrintException(std::current_exception());
  return false;
}

bool
SkysightRequest::DownloadFile(const std::string_view url,
  const Path filename, const DownloadFileType type) noexcept
  try {
  if (!IsLoggedIn())
    return false;

  Net::CurlData *data = nullptr;
  switch (type) {
    case SKYSIGHT_LIVE_REQUEST:
      data = new Net::CurlData;
      data->curl_list = request_headers;
      data->type = Net::FILE;
      break;
    case SKYSIGHT_FORECAST_REQUEST:
      data = new Net::CurlData;
      data->curl_list = request_headers;
      data->type = Net::FILE;
      break;
    case NORMAL_REQUEST:
    default:
      break;
  }

  Net::DownloadManager::Enqueue(url, filename, std::move(data));

#ifdef __AUGUST__
  LogFmt("Request {} -> {} ", url, filename.GetBase().c_str());
#endif

  return true;
}
catch (...) {
  PrintException(std::current_exception());
  return false;
}

bool
SkysightRequest::IsLoggedIn() noexcept
{
  // since 2025/05/08 the key is valid for 1200 sec ( = 20:00min)
  if (valid_until > DateTime::now())
    return true;
  else {
    request_headers->Clear();
    return false;
  }
}

SkysightRequest::SkysightRequest(SkysightAPI* _api,
  const std::string_view _username,
  const std::string_view _password) : 
  api(_api), username(_username), password(_password) 
{
  skysight_listener = new SkysightListener(this);
  request_headers = new CurlSlist();
  RequestCredentialKey();
}

SkysightRequest::~SkysightRequest()
{
  delete skysight_listener;
  delete request_headers;
}
