// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "CoInstance.hpp"
// #include "net/http/CoDownloadToFile.hpp"
#include "net/http/CoDownload.hpp"
#include "net/http/Init.hpp"
#include "system/Args.hpp"
#include "Operation/ConsoleOperationEnvironment.hpp"
#include "util/PrintException.hxx"
#include "time/DateTime.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <iostream>

struct Instance : CoInstance {
  const Net::ScopeInit net_init{GetEventLoop()};
};

//===================================================================
//===================================================================
//===================================================================
//===================================================================

std::string key;
time_t valid_until;
std::vector<boost::json::string> allowed_regions;

void
SetCredentialKey(const boost::json::value &credentials)
try {
  key = credentials.at("key").get_string();
  valid_until = credentials.at("valid_until").to_number<time_t>();

  auto regions = credentials.at("allowed_regions").as_array();
  for (auto region : regions)
    allowed_regions.push_back(region.as_string());
}
catch (...) {
  PrintException(std::current_exception());
}
//===================================================================
//===================================================================
//===================================================================

static Co::InvokeTask
Run(CurlGlobal &curl, const std::string_view url, const std::string_view username,
  const std::string_view password, ProgressListener &progress)
{
  std::array<std::byte, 32> hash;

  const char OpenSoar_ProductToken[] = "OpenSoar v7.43.23"
    " TARGET git=12345678";
  Net::CurlData *data = new Net::CurlData;
  
  CurlSlist cred_slist;
  cred_slist.AppendFormat("%s: %s", "X-API-Key", "OpenSoar");
  cred_slist.AppendFormat("%s: %s", "User-Agent", OpenSoar_ProductToken);
  cred_slist.AppendFormat("%s: %s", "Content-Type", "application/json");
  data->curl_list = &cred_slist;
  
  /* username and password are send with body, not with curl itself */
  // data->username = username;
  // data->password = password;

  char buffer[0xFF];
  std::sprintf(buffer, "{ \"username\":\"%s\",\"password\":\"%s\" }",
    username.data(), password.data());
  data->body = buffer;

  const auto json_body =
    co_await Net::CoDownloadToJson(curl, url.data(),
      data, progress);
  
  SetCredentialKey(json_body);

  std::cout << std::endl;
  std::cout << "key:         " << key << std::endl;
  std::cout << "valid_until: " << valid_until << " / " << 
    DateTime::time_str(valid_until) << std::endl;
  for (auto region : allowed_regions) {
    std::cout << "allowed_regions:  " << region << std::endl;
  }
  std::cout << std::endl;

  delete data;

}

const std::string SKYSIGHTAPI_BASE_URL = "https://skysight.io/api";

int
main(int argc, char **argv) noexcept
try {
  Args args(argc, argv, "USERNAME PASSWORD");
  const char *username = args.ExpectNext();
  const char *password  = args.ExpectNext();
  args.ExpectEnd();

  const std::string url = SKYSIGHTAPI_BASE_URL + "/auth";
  Instance instance;
  ConsoleOperationEnvironment env;

  std::cout << "url:       " << url << std::endl;
  std::cout << "username:  " << username << std::endl;
  instance.Run(Run(*Net::curl, url.c_str(), username, password, env));
  return EXIT_SUCCESS;
} catch (...) {
  PrintException(std::current_exception());
  return EXIT_FAILURE;
}
