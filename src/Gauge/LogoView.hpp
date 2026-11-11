// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/canvas/Bitmap.hpp"
#include "ui/canvas/Font.hpp"
#include "ui/dim/Rect.hpp"

class Canvas;

class LogoView {
  Bitmap logo, big_logo, huge_logo, title, big_title, huge_title;
  Bitmap logo_rgba, big_logo_rgba, huge_logo_rgba;
  Bitmap title_rgba, big_title_rgba, huge_title_rgba;
  Bitmap white_title, huge_white_title;

  Font font;
#ifndef NDEBUG
  Font bold_font;
#endif

public:
  LogoView() noexcept;

  /**
   * Draws the XCSoar logo and the version number into the Canvas.
   *
   * @param canvas the Canvas to draw on
   * @param rc the region within the Canvas to draw into
   * @param dark_mode if true, use light-on-dark colors and white title
   */
  void draw(Canvas &canvas, const PixelRect &rc,
            bool dark_mode = false) noexcept;

  /**
   * Fill @a rc with the background of the start screen, which the
   * progress window keeps while the program loads: a gradient in the
   * colour of the logo with OpenGL; without it there is no alpha
   * blending for the bitmaps, so the background stays white.
   *
   * @return true if the background is dark, i.e. draw() has to be
   * called with dark_mode for the white title
   */
  static bool DrawSplashBackground(Canvas &canvas,
                                   const PixelRect &rc) noexcept;

  /**
   * The height of the controls at the bottom of the start screen: the
   * profile, "Continue" and the bar.  The progress window reserves the
   * same, so the logo does not move when the one follows the other.
   */
  [[gnu::pure]]
  static unsigned GetSplashControlsHeight() noexcept;

  /**
   * The rectangle of control row @a row (0 to 2) at the bottom of the
   * start screen @a rc, centred and not wider than a phone.
   */
  [[gnu::pure]]
  static PixelRect GetSplashRow(const PixelRect &rc, unsigned row) noexcept;
};
