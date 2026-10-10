// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "CachedTopographyRenderer.hpp"
#include "TopographyStore.hpp"

#ifdef ENABLE_OPENGL

void
CachedTopographyRenderer::Draw(Canvas &canvas,
                               const WindowProjection &projection) noexcept
{
  switch (cache.Check(projection, renderer.GetStore().GetSerial())) {
  case MapLayerCache::Action::DIRECT:
    renderer.Draw(canvas, projection);
    return;

  case MapLayerCache::Action::RENDER:
    /* the map scale of the screen, not of the larger buffer: it
       decides which layers are shown */
    renderer.Draw(cache.BeginRender(projection), cache.GetBufferProjection(),
                  projection.GetMapScale());
    cache.EndRender();
    break;

  case MapLayerCache::Action::REUSE:
    break;
  }

  cache.Draw(projection);
}

#else

void
CachedTopographyRenderer::Draw(Canvas &canvas,
                               const WindowProjection &projection) noexcept
{
  if (renderer.GetStore().GetSerial() != last_serial ||
      !cache.Check(projection)) {
    last_serial = renderer.GetStore().GetSerial();

    Canvas &buffer_canvas = cache.Begin(canvas, projection);
    buffer_canvas.ClearWhite();
    renderer.Draw(buffer_canvas, projection);
    cache.Commit(canvas, projection);
  }

  cache.CopyTransparentWhiteTo(canvas, projection);
}

#endif
