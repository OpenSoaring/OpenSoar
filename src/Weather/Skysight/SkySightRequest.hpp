// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "co/InvokeTask.hxx"
#include "co/Task.hxx"
#include "event/Loop.hxx"
#include "event/DeferEvent.hxx"
#include "util/ReturnValue.hxx"
#include "lib/curl/Slist.hxx"

#include <string_view>
#include <boost/json.hpp>

class Path;
class CurlGlobal;
class CurlSlist;
class ProgressListener;
class SkysightAPI;

namespace Co { template<typename T> class Task; }

enum DownloadFileType {
  NORMAL_REQUEST,
  SKYSIGHT_LIVE_REQUEST,
  SKYSIGHT_FORECAST_REQUEST,
};

class SkysightListener;
class SkysightRequest {

private:
  std::string_view key;
  time_t valid_until = 0;
  time_t request_age = 0;
  std::vector<boost::json::string> allowed_regions;
  SkysightListener *skysight_listener = nullptr;

  SkysightAPI *api;
  const std::string_view username;
  const std::string_view password;
  CurlSlist *request_headers = nullptr;

  void OnCompletion(std::exception_ptr error) noexcept;

public:
  SkysightRequest(SkysightAPI *_api,
     const std::string_view _username,
     const std::string_view _password);
  ~SkysightRequest();

  SkysightAPI *GetAPI() { return api; }
  bool SetCredentialKey(const boost::json::value &json);
  bool IsLoggedIn() noexcept;
  bool RequestJson(const std::string_view name, 
    const std::string_view url_part) noexcept;
  bool RequestCredentialKey() noexcept;

  bool DownloadFile(const std::string_view url, const Path filename) noexcept;
  bool DownloadFile(const std::string_view url, const Path filename, 
    const DownloadFileType type) noexcept;
};

