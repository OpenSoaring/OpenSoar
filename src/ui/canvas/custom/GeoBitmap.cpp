// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ui/canvas/Bitmap.hpp"
#include "UncompressedImage.hpp"
#include "Geo/Quadrilateral.hpp"
#include "system/Path.hpp"

#include "LogFile.hpp"

#ifdef USE_GEOTIFF
# include "LibTiff.hpp"
#endif

#include <stdexcept>

GeoQuadrilateral
Bitmap::LoadGeoFile([[maybe_unused]] Path path)
{
#ifdef USE_GEOTIFF
  LogFmt("Bitmap::LoadGeoFile: {}", path.c_str());
  LogFmt("Bitmap::LoadGeoFile: USE_GEOTIFF");
  if (path.EndsWithIgnoreCase(".tif") ||
      path.EndsWithIgnoreCase(".tiff")) {
    auto result = LoadGeoTiff(path);
    if (!LoadFile(path))
      throw std::runtime_error("Failed to use geo image file");

    assert(IsDefined());

    return result.second;
  }
  throw std::runtime_error("Unsupported geo image file");
#else  // USE_GEOTIFF
  LogFmt("Bitmap::LoadGeoFile: {}", path.c_str());
#endif  // USE_GEOTIFF
  return {};

}
