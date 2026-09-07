// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

struct PixelRect;
class Canvas;

namespace SymbolRenderer
{
  enum Direction {
    UP,
    DOWN,
    LEFT,
    RIGHT,
  };

  /** the symbols of a tape deck (replay control) */
  enum class MediaSymbol {
    /** two bars: pause */
    PAUSE,
    /** two triangles pointing left: jump back */
    REWIND,
    /** two triangles pointing right: jump forward */
    FORWARD,
    /** bar and triangle pointing left: back to the start */
    SKIP_START,
    /** triangle pointing right and bar: to the end */
    SKIP_END,
    /** a climbing arrow over the ground line: to the takeoff */
    TAKEOFF,
  };

  void DrawArrow(Canvas &canvas, PixelRect rc, Direction direction,
                 unsigned max_draw_size=0) noexcept;

  void DrawMedia(Canvas &canvas, PixelRect rc, MediaSymbol symbol,
                 unsigned max_draw_size=0) noexcept;
  void DrawSign(Canvas &canvas, PixelRect rc, bool plus,
                unsigned max_draw_size=0) noexcept;
  void DrawHamburger(Canvas &canvas, PixelRect rc) noexcept;
  void DrawBolt(Canvas &canvas, PixelRect rc) noexcept;
}
