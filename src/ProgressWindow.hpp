// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "ui/window/ContainerWindow.hpp"
#include "ui/control/ProgressBar.hpp"
#include "ui/canvas/Bitmap.hpp"
#include "ui/canvas/Color.hpp"
#include "Gauge/LogoView.hpp"
#include "util/StaticString.hxx"

/**
 * The XCSoar splash screen with a progress bar.
 */
class ProgressWindow : public ContainerWindow {
  Color background_color;
  bool dark_mode;

  Bitmap bitmap_progress_border;

#ifndef USE_WINUSER
  Font font;
#endif

  LogoView logo;

  /**
   * The text of SetMessage() on one line; it is shown inside the bar,
   * followed by the percentage.
   */
  StaticString<256> message;

  /**
   * Shows the message and the percentage inside the bar.
   */
  ProgressBar progress_bar;

  unsigned text_height;

  unsigned range_min = 0, range_max = 0;
  /** The default range alone does not mean the job reports progress. */
  bool have_progress_position = false;

  PixelRect logo_position;
  PixelRect bottom_position, progress_bar_position;

public:
  explicit ProgressWindow(ContainerWindow &parent) noexcept;

  void SetMessage(const char *text) noexcept;

  void SetRange(unsigned min_value, unsigned max_value) noexcept;
  void SetStep(unsigned size) noexcept;
  void SetValue(unsigned value) noexcept;
  void Step() noexcept;

private:
  void UpdateLayout(PixelRect rc) noexcept;
  void UpdateBarLabel() noexcept;

protected:
  void OnResize(PixelSize new_size) noexcept override;
  void OnPaint(Canvas &canvas) noexcept override;
};
