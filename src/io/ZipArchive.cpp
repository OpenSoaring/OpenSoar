// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ZipArchive.hpp"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/PathFormatter.hpp"
#include "LogFile.hpp"

#include <zzip/zzip.h>
#include <zlib.h>
#include <vector>

ZipArchive::ZipArchive(Path path)
  :dir(zzip_dir_open(path.c_str(), nullptr))
{
  if (dir == nullptr)
    throw FmtRuntimeError("Failed to open ZIP archive {}", path);
}

ZipArchive::~ZipArchive() noexcept
{
  if (dir != nullptr)
    zzip_dir_close(dir);
}

bool
ZipArchive::Exists(const char *name) const noexcept
{
  ZZIP_STAT st;
  return zzip_dir_stat(dir, name, &st, 0) == 0;
}

std::string
ZipArchive::NextName() noexcept
{
  ZZIP_DIRENT e;
  return zzip_dir_read(dir, &e)
    ? std::string(e.d_name)
    : std::string();
}

bool
ZipIO::UnzipSingleFile(Path zipfile, Path output)
{
  unsigned char unzipBuffer[0x1000];

  gzFile inFileZ = gzopen(zipfile.ToUTF8().c_str(), "rb");
  if (inFileZ == NULL) {
    LogFmt("Error: Failed to gzopen {}", zipfile.ToUTF8().c_str());
    return false;
  }
  std::vector<unsigned char> unzippedData;
  while (true) {
    auto unzippedBytes = gzread(inFileZ, unzipBuffer, 0x1000);
    if (unzippedBytes > 0) {
      unzippedData.insert(unzippedData.end(), unzipBuffer, unzipBuffer + unzippedBytes);
    }
    else {
      break;
    }
  }
  gzclose(inFileZ);

  auto expandfile = fopen(output.ToUTF8().c_str(), "wb");
  fwrite(unzippedData.data(), 1, unzippedData.size(), expandfile);
  fclose(expandfile);
  return true;
}

