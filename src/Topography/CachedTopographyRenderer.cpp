// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "CachedTopographyRenderer.hpp"
#include "TopographyStore.hpp"

#ifdef ENABLE_OPENGL

#include "ui/canvas/opengl/Globals.hpp"

#include <algorithm>
#include <cmath>

/**
 * Do the two projections show the map at the same scale, with the
 * screen center of @a b at most @a max_shift pixels away from that of
 * @a a and turned by at most @a max_angle_degrees?
 */
[[gnu::pure]]
static bool
IsClose(const WindowProjection &a, const WindowProjection &b,
        int max_shift, double max_angle_degrees) noexcept
{
  if (std::fabs(a.GetScale() - b.GetScale()) > a.GetScale() * 1e-4)
    return false;

  if ((a.GetScreenAngle() - b.GetScreenAngle()).AsDelta().AbsoluteDegrees()
      > max_angle_degrees)
    return false;

  const PixelPoint p = a.GeoToScreen(b.GetGeoScreenCenter());
  const PixelPoint c = a.GetScreenCenter();
  return std::abs(p.x - c.x) <= max_shift && std::abs(p.y - c.y) <= max_shift;
}

void
CachedTopographyRenderer::RenderBuffer(const WindowProjection &projection) noexcept
{
  const PixelSize size = projection.GetScreenSize();
  margin = std::max(size.width, size.height) / 10;

  buffer_projection = projection;
  buffer_projection.SetScreenSize({size.width + 2 * margin,
                                   size.height + 2 * margin});
  buffer_projection.SetScreenOrigin(projection.GetScreenOrigin()
                                    + PixelPoint{margin, margin});
  buffer_projection.UpdateScreenBounds();

  const PixelSize buffer_size = buffer_projection.GetScreenSize();
  if (!buffer.IsDefined())
    buffer.Create(buffer_size);
  else
    buffer.Resize(buffer_size);

  buffer.Begin();
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  /* the map scale of the screen, not of the larger buffer: it decides
     which layers are shown */
  renderer.Draw(buffer, buffer_projection, projection.GetMapScale());
  buffer.End();

  last_serial = renderer.GetStore().GetSerial();
  buffer_valid = true;
}

void
CachedTopographyRenderer::DrawBuffer(const WindowProjection &projection) const noexcept
{
  const PixelRect r = buffer_projection.GetScreenRect();
  const PixelPoint corners[] = {
    r.GetTopLeft(), r.GetTopRight(), r.GetBottomLeft(), r.GetBottomRight(),
  };

  BulkPixelPoint quad[4];
  for (unsigned i = 0; i < 4; ++i)
    quad[i] = projection.GeoToScreen(buffer_projection.ScreenToGeo(corners[i]));

  buffer.DrawQuad(quad);
}

void
CachedTopographyRenderer::Draw(Canvas &canvas,
                               const WindowProjection &projection) noexcept
{
  /* while the map keeps moving (zooming, panning, circling with track
     up), drawing into the buffer would only add work; draw directly
     until one frame looks like the one before */
  const bool stable = previous_valid &&
    previous_projection.GetScreenSize() == projection.GetScreenSize() &&
    IsClose(previous_projection, projection, 2, 0.5);
  previous_projection = projection;
  previous_valid = true;

  const PixelSize size = projection.GetScreenSize();
  const bool reusable = buffer_valid &&
    renderer.GetStore().GetSerial() == last_serial &&
    buffer_projection.GetScreenSize() ==
      PixelSize{size.width + 2 * margin, size.height + 2 * margin} &&
    IsClose(buffer_projection, projection, margin / 2, 3);

  if (!reusable) {
    buffer_valid = false;

    if (!stable) {
      renderer.Draw(canvas, projection);
      return;
    }

    RenderBuffer(projection);
  }

  DrawBuffer(projection);
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
