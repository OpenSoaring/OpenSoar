// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

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

struct Instance : CoInstance {
  const Net::ScopeInit net_init{ GetEventLoop() };
};

#if 0
static Co::InvokeTask
DownloadTask(CurlGlobal &curl, const char *url, Path path,
  ProgressListener &progress)
{
  std::array<std::byte, 32> hash;

  const auto response =
    co_await Net::CoDownloadToFile(curl, url, nullptr, nullptr,
      path, &hash, progress);
}
#endif

static Co::InvokeTask
DownloadTask(CurlGlobal &curl, const char *url, Path path,
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

#if 0 // not used up to now
static Co::InvokeTask
DownloadTask(CurlGlobal &curl, const char *url, Path path,
  ProgressListener &progress)
{
  std::array<std::byte, 32> hash;

  const auto response =
    co_await Net::CoDownloadToFile(curl, url,
      nullptr, path, &hash, progress);
  Skysight::GetSkysight()->SetUpdateFlag();
}
#endif

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

// static Instance instance;

void
SkysightCoRequest::SetCredentialKey(const std::string_view _key)
//  const std::string_view _username,
//  const std::string_view _password)
{
  // std::lock_guard<Mutex> lock(mutex);
  // SkysightRequest::SetCredentials(_key, _username, _password);
  key = _key;
/*
  if (!_username.empty())
    username = _username;
  if (!_password.empty())
    password = _password;
*/  
  if (!key.empty()) {
    if (request_headers == nullptr)
      request_headers = new  CurlSlist();
    else
      request_headers->Clear();
    request_headers->AppendFormat("%s: %s", "X-API-Key", key.data());
  }
//  request_headers->AppendFormat("%s: %s", "User-Agent", OpenSoar_ProductToken);
//  request_headers->AppendFormat("%s: %s", "Content-Type", "application/json");
}

#if 0
bool
SkysightCoRequest::DownloadImage(const std::string_view url, const Path filename,
  const std::string_view cred_key) noexcept
try {
  Instance instance;
  PluggableOperationEnvironment env;
  instance.Run(DownloadTask(*Net::curl, url.data(), filename, env));
  return true;
}
catch (...) {
  PrintException(std::current_exception());
  return false;
}
#endif

bool
SkysightCoRequest::RequestCredentialKey(
  const std::string_view user, 
  const std::string_view password) noexcept
try {
  Instance instance;
  PluggableOperationEnvironment env;
  CurlSlist cred_slist;
  //cred_slist.AppendFormat("%s: %s", "X-API-Key", _key.data());
  cred_slist.AppendFormat("%s: %s", "User-Agent", OpenSoar_ProductToken);
  cred_slist.AppendFormat("%s: %s", "Content-Type", "application/json");

  instance.Run(CredentialTask(*Net::curl, user, password, cred_slist, env));
  return true;
}
catch (...) {
  PrintException(std::current_exception());
  return false;
}


bool
SkysightCoRequest::DownloadImage(const std::string_view url, 
  const Path filename, [[maybe_unused]] bool with_auth) noexcept
try {
  Instance instance;
  PluggableOperationEnvironment env;
  // if (with_auth) {
  //   if (request_headers == nullptr)
  //     request_headers = new  CurlSlist();
  //   request_headers->AppendFormat("%s: %s", "X-API-Key", key.data());
  // }

  instance.Run(DownloadTask(*Net::curl, url.data(), filename, request_headers, env));
  return true;
}
catch (...) {
  PrintException(std::current_exception());
  return false;
}


SkysightCoRequest::SkysightCoRequest(const std::string_view _key) 
{
  SetCredentialKey(_key);
}
