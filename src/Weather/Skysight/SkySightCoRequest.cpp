// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef WITH_COREQUEST

#include "SkySightCoRequest.hpp"
#include "Skysight.hpp"
#include "SkysightAPI.hpp"

#include "net/http/CoDownloadToFile.hpp"
#include "net/http/Init.hpp"
#include "Operation/PluggableOperationEnvironment.hpp"
#include "Operation/ProgressListener.hpp"
#include "util/StaticString.hxx"
#include "util/PrintException.hxx"
#include "system/Path.hpp"
#include "LocalPath.hpp"
#include "Version.hpp"

#include <string>

// TODO(August2111): Not functional in the moment!

static Co::InvokeTask
DownloadTask(CurlGlobal &curl, const char *url, Path path,
  ProgressListener &progress)
{
  std::array<std::byte, 32> hash;

  const auto response =
    co_await Net::CoDownloadToFile(curl, url, nullptr, nullptr,
      path, &hash, progress);
}

static Co::InvokeTask
_DownloadTask(CurlGlobal &curl, const char *url, Path path,
  CurlSlist *slist, ProgressListener &progress)
{
  std::array<std::byte, 32> hash;

#ifdef _DEBUG
  const auto response =
#endif
    co_await Net::CoDownloadToFile(curl, url,
      slist, path, &hash, progress);
  Skysight::GetSkysight()->SetUpdateFlag();
}

static Co::InvokeTask
CredentialTask(CurlGlobal &curl,
  [[maybe_unused]] const std::string_view user,
  [[maybe_unused]] const std::string_view password,
  CurlSlist &slist, ProgressListener &progress)
{
  std::array<std::byte, 32> hash;

  StaticString<256> url;
  url.Format("%s/auth", SKYSIGHTAPI_BASE_URL);

#ifdef _DEBUG
  const auto response =
#endif
    co_await Net::CoDownloadToFile(curl, url.c_str(),
      &slist, Path("Credential"), &hash, progress);
  Skysight::GetSkysight()->SetUpdateFlag();
}

void
SkysightCoRequest::SetCredentialKey(const std::string_view _key,
  time_t _expire_time)
{
  key = _key;
  expire_time = _expire_time;
  if (!key.empty()) {
    if (request_headers == nullptr)
      request_headers = new  CurlSlist();
    else
      request_headers->Clear();
    request_headers->AppendFormat("%s: %s", "X-API-Key", key.data());
  }
}

bool
SkysightCoRequest::RequestCredentialKey() noexcept
try {
  PluggableOperationEnvironment env;
  CurlSlist cred_slist;
  //cred_slist.AppendFormat("%s: %s", "X-API-Key", _key.data());
  cred_slist.AppendFormat("%s: %s", "User-Agent", OpenSoar_ProductToken);
  cred_slist.AppendFormat("%s: %s", "Content-Type", "application/json");
  CredentialTask(*Net::curl, username, password, cred_slist, env);
  return true;
}
catch (...) {
  PrintException(std::current_exception());
  return false;
}

Co::Task<Path>
CoDownloadImage(const std::string_view url,
  const Path filename, CurlSlist *request_headers, CurlGlobal &curl,
  ProgressListener &progress) noexcept
{
  // co_await -> ???
    _DownloadTask(curl, url.data(), filename, request_headers, progress);
  co_return std::move(filename);
}

bool
SkysightCoRequest::DownloadImage(const std::string_view url, 
  const Path filename, [[maybe_unused]] bool with_auth) noexcept
try {
  PluggableOperationEnvironment env;
  auto task = CoDownloadImage(url.data(), filename, request_headers, *Net::curl, env);
  ReturnValue<Path> value;
  auto invoke = [&value](Co::Task<Path> task) -> Co::InvokeTask {
    value.Set(co_await task);
  };
  invoke(std::move(task));
  return true;
}
catch (...) {
  PrintException(std::current_exception());
  return false;
}


SkysightCoRequest::SkysightCoRequest(
  const std::string_view _username,
  const std::string_view _password) : 
  username(_username), password(_password) 
{
  // RequestCredentialKey();
}
#endif  // WITH_COREQUEST
