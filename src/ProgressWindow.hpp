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
 * The splash screen with a progress bar.  The loading after the start
 * screen draws it in the look of the start screen (see
 * LogoView::DrawSplashBackground()); a download or another job keeps
 * the plain technical look, the logo on the dialog background above a
 * bar in a frame.
 */
class ProgressWindow : public ContainerWindow {
  /** the look of the start screen instead of the technical one */
  const bool splash;

  Color background_color;
  bool dark_mode;

  /** the technical look only: the frame around the bar */
  Bitmap bitmap_progress_border;

  /** the technical look only: sizes the bar for the message */
  Font font;

  LogoView logo;

  /**
   * The text of SetMessage() on one line; it is shown inside the bar,
   * followed by the percentage.
   */
  StaticString<256> message;

  ProgressBar progress_bar;

  unsigned text_height = 0;

  unsigned range_min = 0, range_max = 0;
  /** The default range alone does not mean the job reports progress. */
  bool have_progress_position = false;

  PixelRect logo_position;
  PixelRect bottom_position, progress_bar_position;

public:
  /**
   * @param splash draw in the look of the start screen (the loading
   * after it), not in the technical one (downloads and other jobs)
   */
  explicit ProgressWindow(ContainerWindow &parent,
                          bool splash = false) noexcept;

  void SetMessage(const char *text) noexcept;

  void SetRange(unsigned min_value, unsigned max_value) noexcept;
  void SetStep(unsigned size) noexcept;
  void SetValue(unsigned value) noexcept;
  void Step() noexcept;

private:
  void UpdateLayout(PixelRect rc) noexcept;
  void UpdateBarLabel(unsigned value) noexcept;

protected:
  void OnResize(PixelSize new_size) noexcept override;
  void OnPaint(Canvas &canvas) noexcept override;
};
