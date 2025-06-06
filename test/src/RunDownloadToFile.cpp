// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "CoInstance.hpp"
#include "net/http/CoDownload.hpp"
#include "net/http/Init.hpp"
#include "system/Args.hpp"
#include "Operation/ConsoleOperationEnvironment.hpp"
#include "util/PrintException.hxx"

#include <stdio.h>
#include <stdlib.h>
#include <iostream>

struct Instance : CoInstance {
  const Net::ScopeInit net_init{GetEventLoop()};
};

static void
HexPrint(std::span<const std::byte> b) noexcept
{
  for (auto i : b)
    printf("%02x", (uint8_t)i);
}

static Co::InvokeTask
Run(CurlGlobal &curl, const char *url, Path path,
    ProgressListener &progress)
{
  std::array<std::byte, 32> hash;

  Net::CurlData *data = new Net::CurlData;
  data->hash = &hash;
  const auto response =
    co_await Net::CoDownloadToFile(curl, url, path, data, progress);
  HexPrint(hash);
  delete data;
  printf("\n");
}

int
main(int argc, char **argv) noexcept
try {
  Args args(argc, argv, "URL PATH");
  const char *url = args.ExpectNext();
  const auto path = args.ExpectNextPath();
  args.ExpectEnd();

  std::cout << "url:   " << url << std::endl;
  std::cout << "path:  " << path.c_str() << std::endl;

  Instance instance;
  ConsoleOperationEnvironment env;
  instance.Run(Run(*Net::curl, url, path, env));
  return EXIT_SUCCESS;
} catch (...) {
  PrintException(std::current_exception());
  return EXIT_FAILURE;
}
