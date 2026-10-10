// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "TopographyRenderer.hpp"
#include "Renderer/TransparentRendererCache.hpp"

#ifdef ENABLE_OPENGL
#include "Renderer/MapLayerCache.hpp"
#endif

/**
 * Class used to manage and render vector topography layers
 */
class CachedTopographyRenderer {
  TopographyRenderer renderer;

#ifdef ENABLE_OPENGL
  /* Drawing all visible shapes takes about half a second per frame on
     an OpenVario (Mali-400) when zoomed out far, and the map is
     redrawn with every GPS fix even if nothing moves; see
     #MapLayerCache. */
  MapLayerCache cache;
#else
  TransparentRendererCache cache;

  unsigned last_serial = 0;
#endif

public:
  CachedTopographyRenderer(const TopographyStore &store,
                           const TopographyLook &look) noexcept
    :renderer(store, look)
  {}

  void Flush() noexcept {
    cache.Invalidate();
  }

  void Draw(Canvas &canvas, const WindowProjection &projection) noexcept;


  void DrawLabels(Canvas &canvas, const WindowProjection &projection,
                  LabelBlock &label_block) noexcept {
    renderer.DrawLabels(canvas, projection, label_block);
  }
};
