// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/window/PaintWindow.hpp"

#include <string>

class Font;

class ProgressBar : public PaintWindow {
  unsigned min_value = 0, max_value = 0, value = 0, step_size = 1;

  /**
   * Drawn centred inside the bar, light on the filled part and dark on
   * the rest.
   */
  std::string text;

  const Font *font = nullptr;

public:
  void SetRange(unsigned min_value, unsigned max_value);

  unsigned GetValue() const {
    return value;
  }

  void SetValue(unsigned value);
  void SetStep(unsigned size);
  void Step();

  /**
   * Show a text inside the bar; nullptr or an empty string hides it.
   */
  void SetText(const char *text) noexcept;

  /**
   * The font for the text; without one the canvas keeps its own.  The
   * object must live as long as this window.
   */
  void SetFont(const Font &_font) noexcept {
    font = &_font;
  }

protected:
  void OnPaint(Canvas &canvas) noexcept override;
};
