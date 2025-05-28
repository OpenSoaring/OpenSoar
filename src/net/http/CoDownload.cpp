// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "CoDownload.hpp"
#include "Progress.hpp"
#include "lib/curl/Setup.hxx"
#include "lib/curl/Mime.hxx"
#include "lib/curl/CoStreamRequest.hxx"
#include "lib/curl/Slist.hxx"
#include "lib/sodium/SHA256.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "io/DigestOutputStream.hxx"
#include "io/FileOutputStream.hxx"
#include "system/Path.hpp"
#include "system/FileUtil.hpp"

#include <cassert>
#include <optional>


#include "lib/curl/CoStreamRequest.hxx"
#include "json/ParserOutputStream.hxx"

namespace Net {

Co::EagerTask<boost::json::value>
CoDownloadToJson(CurlGlobal &curl, const std::string_view url,
  const Net::CurlData *data, ProgressListener &progress)
{
  CurlEasy easy{ url.data() };
  Curl::Setup(easy);
  const Net::ProgressAdapter progress_adapter{ easy, progress };

  if (data && data->mime_map) {
    CurlMime mime{ easy.Get() };

    for (auto entry : *data->mime_map)
       mime.Add(entry.first.data()).Data(entry.second.data());

    easy.SetMimePost(mime.get());
   }

  if (data && !data->username.empty())
    easy.SetOption(CURLOPT_USERNAME, data->username.c_str());
  if (data && !data->password.empty())
    easy.SetOption(CURLOPT_PASSWORD, data->password.c_str());

  if (data && !data->body.empty()) {
    easy.SetRequestBody(data->body.c_str());
  }

  if (data && data->curl_list)
    easy.SetRequestHeaders(data->curl_list->Get());
  
  easy.SetFailOnError(false);
  easy.SetVerifyPeer(false);

  Json::ParserOutputStream parser;
  const auto response =
    co_await Curl::CoStreamRequest(curl, std::move(easy), parser);
  if ((response.status < 200) || (response.status > 201))
    throw FmtRuntimeError("CoDownloadToJson {} status {}", url, response.status);

  co_return parser.Finish();
}

Co::EagerTask<Curl::CoResponse>
CoDownloadToFile(CurlGlobal &curl, const std::string_view url,
  const Path path,
  const Net::CurlData *data, ProgressListener &progress)
{
  assert(!url.empty());
  //  assert(data);
  assert(!path.empty());

  FileOutputStream file(path);
  OutputStream *os = &file;

  std::optional<DigestOutputStream<SHA256State>> digest;
  if (data && data->hash)
    os = &digest.emplace(*os);

  CurlEasy easy{ url.data() };
  Curl::Setup(easy);
  const Net::ProgressAdapter progress_adapter{ easy, progress };

  if (data) {
    // data is defined:
    if (data->mime_map && !data->mime_map->empty()) {
      CurlMime mime{ easy.Get() };

      for (auto entry : *data->mime_map)
        mime.Add(entry.first.data()).Data(entry.second.data());

      easy.SetMimePost(mime.get());
    }

    if (!data->username.empty())
      easy.SetOption(CURLOPT_USERNAME, data->username.c_str());
    if (!data->password.empty())
      easy.SetOption(CURLOPT_PASSWORD, data->password.c_str());

    if (!data->body.empty()) {
      easy.SetRequestBody(data->body.c_str());
    }

    if (data->curl_list) {
      easy.SetRequestHeaders(data->curl_list->Get());
    }
  }
 
  easy.SetFailOnError(false);
  easy.SetVerifyPeer(false);
  auto  response =
      co_await Curl::CoStreamRequest(curl, std::move(easy), *os);

  switch (response.status) {

      case 200:
      case 201: {
        [[maybe_unused]] auto file_size = file.Tell();
          file.Commit();
          if (data && data->hash)
            digest->Final(std::span{ *data->hash });
        }
        break;
      case 401: {  // 'Unauthorized'
        auto file_size = file.Tell();
        if (file_size > 0) {
          file.Commit();  // mostly this is a json file with error description
          File::Rename(file.GetPath(), file.GetPath().WithSuffix(".json"));
        }
        throw FmtRuntimeError("CoDownloadToFile {} status {}", url,
          response.status);
      }
      break;
      default: // other error
        throw FmtRuntimeError("CoDownloadToFile {} status {}", url,
          response.status);
    }
    co_return response;
}

} // namespace Net
