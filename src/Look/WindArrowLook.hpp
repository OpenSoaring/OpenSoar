// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/canvas/Pen.hpp"
#include "ui/canvas/Brush.hpp"

class Font;

struct WindArrowLook
{
  Pen arrow_pen, shaft_pen;

  /** the wind estimated by XCSoar itself */
  Brush arrow_brush;

  /** the average wind received from an external sensor */
  Brush arrow_brush_external;

  /** the instantaneous (live) wind received from an external sensor */
  Brush arrow_brush_instantaneous;

  const Font *font;

  void Initialise(const Font &font, bool inverse = false);
};
