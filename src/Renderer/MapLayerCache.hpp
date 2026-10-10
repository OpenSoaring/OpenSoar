// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#ifdef ENABLE_OPENGL

#include "Projection/WindowProjection.hpp"
#include "ui/canvas/BufferCanvas.hpp"
#include "ui/canvas/opengl/Globals.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

/**
 * Keeps an expensive map layer (topography, airspace) as an image, so
 * that it does not have to be drawn again with every frame.
 *
 * On a slow device with OpenGL (an OpenVario with its Mali-400) a
 * layer can take half a second to draw when zoomed out far, and the
 * map is redrawn with every GPS fix even if nothing moves.  The layer
 * is therefore drawn into a texture that covers the screen plus a
 * margin of a tenth of its size, and the texture is drawn as a moved
 * and rotated quad as long as the scale is unchanged, the map has
 * moved less than half the margin and turned by less than 3 degrees,
 * and the content key of the layer is the same.  The map uses a Web
 * Mercator projection, which is linear for a given Mercator scale, so
 * a moved image is exact as long as that scale does not change.
 *
 * While the map keeps changing from frame to frame (zooming, panning,
 * circling with track up) the caller is told to draw directly,
 * because filling the larger buffer would only add work.
 *
 * Header only, because the layers live in different libraries.
 */
class MapLayerCache {
  /** the cached image, with alpha so the layers below show through */
  BufferCanvas buffer{true};

  /** the projection #buffer was drawn with (screen plus margin) */
  WindowProjection buffer_projection;

  /** the projection of the previous frame */
  WindowProjection previous_projection;

  /** what the layer depended on when #buffer was drawn */
  uint64_t buffer_key = 0;

  /** the margin around the screen in #buffer, in pixels */
  int margin = 0;

  bool buffer_valid = false, previous_valid = false;

  /**
   * Was the layer drawn with premultiplied alpha (see
   * #BlendPremultiplied())?
   */
  const bool premultiplied;

public:
  enum class Action {
    /** draw the layer directly onto the screen */
    DIRECT,

    /** draw the layer between BeginRender() and EndRender(), then
        call Draw() */
    RENDER,

    /** only call Draw() */
    REUSE,
  };

  /**
   * @param _premultiplied the layer is drawn with semi-transparent
   * colours, and its renderer blends them with #BlendPremultiplied()
   */
  explicit MapLayerCache(bool _premultiplied=false) noexcept
    :premultiplied(_premultiplied) {}

  void Invalidate() noexcept {
    buffer_valid = false;
  }

  /**
   * Decide what to do for this frame.
   *
   * @param key a value that changes whenever the content of the layer
   * changes for a reason other than the projection
   */
  Action Check(const WindowProjection &projection, uint64_t key) noexcept {
    const bool stable = previous_valid &&
      previous_projection.GetScreenSize() == projection.GetScreenSize() &&
      IsClose(previous_projection, projection, 2, 0.5);
    previous_projection = projection;
    previous_valid = true;

    const PixelSize size = projection.GetScreenSize();
    if (buffer_valid && key == buffer_key &&
        buffer_projection.GetScreenSize() ==
        PixelSize{size.width + 2 * margin, size.height + 2 * margin} &&
        IsClose(buffer_projection, projection, margin / 2, 3))
      return Action::REUSE;

    buffer_valid = false;
    if (!stable)
      return Action::DIRECT;

    buffer_key = key;
    return Action::RENDER;
  }

  /**
   * Start drawing into the buffer.  Draw the layer into the returned
   * canvas with GetBufferProjection().
   */
  Canvas &BeginRender(const WindowProjection &projection) noexcept {
    const PixelSize size = projection.GetScreenSize();
    margin = std::max(size.width, size.height) / 10;

    buffer_projection = projection;
    buffer_projection.SetScreenSize({size.width + 2 * margin,
                                     size.height + 2 * margin});
    buffer_projection.SetScreenOrigin(projection.GetScreenOrigin()
                                      + PixelPoint{margin, margin});
    buffer_projection.UpdateScreenBounds();

    /* a new buffer instead of a resized one: the Mali-400 driver
       of the OpenVario keeps drawing into the old storage of a texture
       that is redefined while attached to a framebuffer, so after the
       map had changed its size (a page with a cross section below the
       map) the cache kept showing an outdated, stretched image */
    const PixelSize buffer_size = buffer_projection.GetScreenSize();
    if (!buffer.IsDefined() || buffer.GetSize() != buffer_size) {
      buffer.Destroy();
      buffer.Create(buffer_size);
    }

    buffer.Begin();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    return buffer;
  }

  const WindowProjection &GetBufferProjection() const noexcept {
    return buffer_projection;
  }

  void EndRender() noexcept {
    buffer.End();
    buffer_valid = true;
  }

  /**
   * Draw the buffer onto the current target, moved and rotated to
   * match @a projection.
   */
  void Draw(const WindowProjection &projection) const noexcept {
    const PixelRect r = buffer_projection.GetScreenRect();
    const PixelPoint corners[] = {
      r.GetTopLeft(), r.GetTopRight(), r.GetBottomLeft(), r.GetBottomRight(),
    };

    BulkPixelPoint quad[4];
    for (unsigned i = 0; i < 4; ++i)
      quad[i] = projection.GeoToScreen(buffer_projection.ScreenToGeo(corners[i]));

    buffer.DrawQuad(quad, premultiplied);
  }

  /**
   * The blend function for drawing semi-transparent colours into a
   * buffer that starts transparent: the colour is stored
   * premultiplied with its alpha and the alpha is accumulated, so
   * that the buffer can later be blended exactly as the layer would
   * have been drawn directly.  Drawn directly onto the screen, it
   * gives the same colours as GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA.
   */
  static void BlendPremultiplied() noexcept {
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  }

  /** mix @a value into a content key */
  static constexpr uint64_t Mix(uint64_t key, uint64_t value) noexcept {
    return (key ^ value) * 0x100000001b3ULL;
  }

  static constexpr uint64_t KEY_START = 0xcbf29ce484222325ULL;

private:
  /**
   * Do the two projections show the map at the same scale, with the
   * screen center of @a b at most @a max_shift pixels away from that
   * of @a a and turned by at most @a max_angle_degrees?
   */
  [[gnu::pure]]
  static bool IsClose(const WindowProjection &a, const WindowProjection &b,
                      int max_shift, double max_angle_degrees) noexcept {
    if (std::fabs(a.GetScale() - b.GetScale()) > a.GetScale() * 1e-4)
      return false;

    /* the projection keeps the ground scale and derives the Mercator
       scale from the latitude of the screen origin, so moving the map
       north or south stretches a cached image slightly; allow at most
       half a pixel at the far edge */
    const auto size = a.GetScreenSize();
    if (std::fabs(a.GetMercatorScale() - b.GetMercatorScale()) >
        a.GetMercatorScale() * 0.5 / std::max(size.width, size.height))
      return false;

    if ((a.GetScreenAngle() - b.GetScreenAngle()).AsDelta().AbsoluteDegrees()
        > max_angle_degrees)
      return false;

    const PixelPoint p = a.GeoToScreen(b.GetGeoScreenCenter());
    const PixelPoint c = a.GetScreenCenter();
    return std::abs(p.x - c.x) <= max_shift &&
      std::abs(p.y - c.y) <= max_shift;
  }
};

#endif /* ENABLE_OPENGL */
