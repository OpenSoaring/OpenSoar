// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Canvas.hpp"
#include "Math/Point2D.hpp"
#include "ui/opengl/Features.hpp" // for SOFTWARE_ROTATE_DISPLAY

#include <glm/mat4x4.hpp>

#ifdef SOFTWARE_ROTATE_DISPLAY
#include <cstdint>
enum class DisplayOrientation : uint8_t;
#endif

class GLTexture;
class GLFrameBuffer;
class GLRenderBuffer;

/**
 * An off-screen #Canvas implementation.
 */
class BufferCanvas : public Canvas {
  static constexpr GLint TYPE = GL_UNSIGNED_BYTE;

  /**
   * GL_RGB, or GL_RGBA for a buffer that keeps an alpha channel (see
   * the constructor).  OpenGL/ES requires the internal format to
   * equal the format.
   */
  GLint format = GL_RGB;

  GLTexture *texture = nullptr;

  GLFrameBuffer *frame_buffer = nullptr;

  GLRenderBuffer *stencil_buffer = nullptr;

  GLint old_viewport[4];

  glm::mat4 old_projection_matrix;

  PixelPoint old_translate;
  UnsignedPoint2D old_size;

  /**
   * Screen-space scissor must not clip FBO draws (e.g. #VScrollPanel
   * leaves GL_SCISSOR_TEST enabled while child OnPaint fills a tall
   * buffer).
   */
  GLboolean old_scissor_enabled = GL_FALSE;

#ifdef SOFTWARE_ROTATE_DISPLAY
  DisplayOrientation old_orientation;
#endif

#ifndef NDEBUG
  bool active = false;
#endif

public:
  BufferCanvas() noexcept = default;

  /**
   * @param with_alpha keep an alpha channel, so that what is painted
   * into the buffer can later be blended over other content with
   * #DrawQuad(); the buffer starts transparent only after the caller
   * cleared it with alpha 0
   */
  explicit BufferCanvas(bool with_alpha) noexcept
    :format(with_alpha ? GL_RGBA : GL_RGB) {}

  ~BufferCanvas() noexcept {
    Destroy();
  }

  bool IsDefined() const noexcept {
    return texture != nullptr;
  }

  void Create(PixelSize new_size) noexcept;

  void Create([[maybe_unused]] const Canvas &canvas, PixelSize new_size) noexcept {
    assert(canvas.IsDefined());

    Create(new_size);
  }

  void Create(const Canvas &canvas) noexcept {
    Create(canvas, canvas.GetSize());
  }

  void Destroy() noexcept;

  void Resize(PixelSize new_size) noexcept;

  /**
   * Similar to Resize(), but never shrinks the buffer.
   */
  void Grow(PixelSize new_size) noexcept;

  /**
   * Begin painting into this buffer's current size (no resize).
   * Pair with #End.  Does not copy to any on-screen canvas.
   */
  void Begin() noexcept;

  /**
   * Begin painting to the buffer, resizing it to match @a other.
   *
   * @param other an on-screen #Canvas
   */
  void Begin(Canvas &other) noexcept;

  /**
   * Finish painting started with #Begin; restores GL state.
   * Does not blit to the screen.
   */
  void End() noexcept;

  /**
   * Commit the data that was painted into this #BufferCanvas into
   * both the buffer and the other #Canvas.
   *
   * This method must be called before the next Begin().  There is no
   * rollback method, and painting to the buffer may be destructive
   * for the "other" #Canvas until Commit() is called.
   *
   * @param other an on-screen #Canvas
   */
  void Commit(Canvas &other) noexcept;

  void CopyTo(Canvas &other) noexcept;

  /**
   * Draw the whole buffer onto the current target, stretched onto the
   * quadrilateral given by its four corners (top left, top right,
   * bottom left, bottom right of the buffer), blended by the buffer's
   * alpha channel.  This allows drawing a cached image moved and
   * rotated.
   *
   * @param premultiplied the colours in the buffer are premultiplied
   * with their alpha (semi-transparent content drawn with
   * #ScopeAlphaBlend into a buffer cleared to transparent)
   */
  void DrawQuad(const BulkPixelPoint corners[4],
                bool premultiplied=false) const noexcept;

  /**
   * Copy a source rectangle from this buffer onto @a dest.
   */
  void CopyTo(Canvas &dest, PixelRect dest_rc,
              PixelRect src_rc) noexcept;

private:
  void Activate() noexcept;
  void Deactivate() noexcept;
};
