// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/opengl/System.hpp"

/**
 * Enables and auto-disables an OpenGL capability.
 */
template<GLenum cap>
class GLEnable {
public:
  [[nodiscard]]
  GLEnable() noexcept {
    ::glEnable(cap);
  }

  ~GLEnable() noexcept {
    ::glDisable(cap);
  }

  GLEnable(const GLEnable &) = delete;
  GLEnable &operator=(const GLEnable &) = delete;
};

class GLBlend : public GLEnable<GL_BLEND> {
public:
  [[nodiscard]]
  GLBlend(GLenum sfactor, GLenum dfactor) noexcept {
    ::glBlendFunc(sfactor, dfactor);
  }

  [[nodiscard]]
  GLBlend(GLclampf alpha) noexcept {
    ::glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
    ::glBlendColor(0, 0, 0, alpha);
  }
};

/**
 * Enable alpha blending with source's alpha value (the most common
 * variant of GL_BLEND).
 *
 * The alpha channel of the target is blended separately (source
 * over destination), so that drawing into a transparent off-screen
 * buffer leaves the colour premultiplied and the coverage in alpha,
 * and the buffer can be blended onto the screen later with the same
 * result as drawing there directly.  On the screen the colours are
 * the same as with glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA).
 */
class ScopeAlphaBlend : GLEnable<GL_BLEND> {
public:
  [[nodiscard]]
  ScopeAlphaBlend() noexcept {
    ::glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                          GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  }
};

class GLScissor : public GLEnable<GL_SCISSOR_TEST> {
public:
  [[nodiscard]]
  GLScissor(GLint x, GLint y, GLsizei width, GLsizei height) noexcept {
    ::glScissor(x, y, width, height);
  }
};
