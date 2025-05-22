// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#define SKYSIGHT_PREPERED 0

#if SKYSIGHT_PREPERED
#include "Terrain/RasterRenderer.hpp"

class SkysightCache;
class SkysightStore;

#ifndef ENABLE_OPENGL
#include "Projection/CompareProjection.hpp"
#endif

struct TerrainRendererSettings;

class SkysightRenderer {
  SkysightCache cache;

  RasterRenderer raster_renderer;

#ifndef ENABLE_OPENGL
  CompareProjection compare_projection;
#endif

  const ColorRamp *last_color_ramp = nullptr;

public:
  SkysightRenderer(const SkysightStore &_store, unsigned parameter)
    :cache(_store, parameter) {}

  /**
   * Flush the cache.
   */
  void Flush() {
#ifdef ENABLE_OPENGL
    raster_renderer.Invalidate();
#else
    compare_projection.Clear();
#endif
  }

  unsigned GetParameter() const {
    return cache.GetParameter();
  }

  /**
   * Returns the human-readable name for the current RASP map, or
   * nullptr if no RASP map is enabled.
   */
  [[gnu::pure]]
  const char *GetLabel() const {
    return cache.GetMapLabel();
  }

  [[gnu::pure]]
  bool IsInside(GeoPoint p) const {
    return cache.IsInside(p);
  }

  void SetTime(time_t t) {
    cache.SetTime(t);
  }

  void Update(time_t time_local, OperationEnvironment &operation) {
    cache.Reload(time_local, operation);
  }

  /**
   * @return true if an image has been rendered and Draw() may be
   * called
   */
  bool Generate(const WindowProjection &projection,
                const TerrainRendererSettings &settings);

  void Draw(Canvas &canvas, const WindowProjection &projection) const {
    raster_renderer.Draw(canvas, projection, true);
  }
};
#endif