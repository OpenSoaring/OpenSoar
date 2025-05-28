// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "lib/curl/CoRequest.hxx"
#include "co/Task.hxx"
#include "lib/curl/Slist.hxx"

#include <string>
#include <array>
#include <cstddef> // for std::byte
#include <boost/json.hpp>

class Path;
class ProgressListener;

namespace Net {

  enum DownloadType {
    FILE,
    JSON,
    BUFFER
  };


  struct CurlData {
    std::string name;

    std::string username;
    std::string password;

    std::map <std::string_view, std::string_view > *mime_map = nullptr;
    CurlSlist *curl_list = nullptr;
    std::string body;
    bool set_fail_on_error = true;
    std::array<std::byte, 32> *hash = nullptr;  // w/o hash value

    Net::DownloadType type = Net::DownloadType::FILE;
    boost::json::value *json_value = nullptr;
  };

/**
 * Download a URL into the specified file.
 *
 * Throws on error.
 */
Co::EagerTask<boost::json::value>
CoDownloadToJson(CurlGlobal &curl, const std::string_view url,
  const Net::CurlData *data, ProgressListener &progress);

Co::EagerTask<Curl::CoResponse>
CoDownloadToFile(CurlGlobal &curl, const std::string_view url,
  // const std::string_view name, const Net::CurlData *data,
  const Path path, const Net::CurlData *data,
  ProgressListener &progress);

} // namespace Net
