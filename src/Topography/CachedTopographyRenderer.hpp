// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "TopographyRenderer.hpp"
#include "Renderer/TransparentRendererCache.hpp"

#ifdef ENABLE_OPENGL
#include "Projection/WindowProjection.hpp"
#include "ui/canvas/BufferCanvas.hpp"
#endif

/**
 * Class used to manage and render vector topography layers
 */
class CachedTopographyRenderer {
  TopographyRenderer renderer;

#ifdef ENABLE_OPENGL
  /* Drawing all visible shapes takes about half a second per frame on
     an OpenVario (Mali-400) when zoomed out far, and the map is
     redrawn with every GPS fix even if nothing moves, which keeps one
     core busy all the time and delays key presses by seconds.  The
     topography is therefore drawn into a texture with a margin around
     the screen, and that texture is reused, moved and rotated, while
     the scale stays the same and the map has moved less than half the
     margin.  The map uses a Web Mercator projection, which is linear,
     so a moved copy is exact. */

  /** the cached image, with alpha so terrain shows through */
  BufferCanvas buffer{true};

  /** the projection #buffer was drawn with (screen size including
      the margin) */
  WindowProjection buffer_projection;

  /** the projection of the previous frame */
  WindowProjection previous_projection;

  unsigned last_serial = 0;

  /** the margin around the screen in #buffer, in pixels */
  int margin = 0;

  bool buffer_valid = false, previous_valid = false;
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
#ifdef ENABLE_OPENGL
    buffer_valid = false;
#else
    cache.Invalidate();
#endif
  }

  void Draw(Canvas &canvas, const WindowProjection &projection) noexcept;

#ifdef ENABLE_OPENGL
private:
  void RenderBuffer(const WindowProjection &projection) noexcept;
  void DrawBuffer(const WindowProjection &projection) const noexcept;

public:
#endif

  void DrawLabels(Canvas &canvas, const WindowProjection &projection,
                  LabelBlock &label_block) noexcept {
    renderer.DrawLabels(canvas, projection, label_block);
  }
};
