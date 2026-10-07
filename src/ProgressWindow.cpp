// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ProgressWindow.hpp"
#include "GlobalSettings.hpp"
#include "Look/Colors.hpp"
#include "Screen/Layout.hpp"
#include "Look/FontDescription.hpp"
#include "Resources.hpp"

#include <algorithm>
#include <cstdint>

#ifdef USE_WINUSER
#include "ui/canvas/AnyCanvas.hpp"
#else
#include "ui/canvas/Canvas.hpp"
#endif

ProgressWindow::ProgressWindow(ContainerWindow &parent) noexcept
  :background_color(GlobalSettings::dark_mode
                    ? COLOR_DARK_THEME_BACKGROUND : COLOR_WHITE),
   dark_mode(GlobalSettings::dark_mode)
{
  message.clear();

  PixelRect rc = parent.GetClientRect();
  WindowStyle style;
  style.Hide();
  Create(parent, rc, style);

  // Load progress bar background
  bitmap_progress_border.Load(IDB_PROGRESSBORDER);

  // Determine text height
#ifndef USE_WINUSER
  font.Load(FontDescription(Layout::FontScale(10)));
  text_height = font.GetHeight();
#else
  {
    AnyCanvas canvas;
    text_height = canvas.GetFontHeight();  }
#endif

  UpdateLayout(GetClientRect());

  // Initialize progress bar
  progress_bar.Create(*this, progress_bar_position);
#ifndef USE_WINUSER
  progress_bar.SetFont(font);
#endif

  // Set progress bar step size and range
  SetRange(0, 1000);
  SetStep(50);

  // Show dialog
  ShowOnTop();
}

void
ProgressWindow::UpdateLayout(PixelRect rc) noexcept
{
  const unsigned height = rc.GetHeight();

  /* Make progress bar height proportional to window height, but high
     enough for the message, which is drawn inside the bar */
  const unsigned progress_height =
    std::max(height / 20, text_height + text_height / 2);
  const unsigned progress_horizontal_border = progress_height / 2;
  const unsigned progress_border_height = progress_height * 2;

  logo_position = rc;
  logo_position.bottom -= progress_border_height;

  bottom_position = rc;
  bottom_position.top = bottom_position.bottom - progress_border_height;

  progress_bar_position.left = bottom_position.left + progress_horizontal_border;
  progress_bar_position.right = bottom_position.right - progress_horizontal_border;
  progress_bar_position.top = bottom_position.top + progress_horizontal_border;
  progress_bar_position.bottom = bottom_position.bottom - progress_horizontal_border;
}

void
ProgressWindow::SetMessage(const char *text) noexcept
{
  AssertThread();

  /* the bar has room for one line only */
  message = text != nullptr ? text : "";
  for (auto *p = message.data(); *p != '\0'; ++p)
    if (*p == '\n' || *p == '\r')
      *p = ' ';

  UpdateBarLabel();
}

void
ProgressWindow::UpdateBarLabel() noexcept
{
  if (!have_progress_position || range_max <= range_min) {
    progress_bar.SetText(message.c_str());
    return;
  }

  const unsigned value =
    std::clamp(progress_bar.GetValue(), range_min, range_max);
  const unsigned percent = static_cast<unsigned>
    (uint64_t{value - range_min} * 100u / (range_max - range_min));

  /* A number and a percent sign; nothing to translate. */
  StaticString<300> label;
  if (message.empty())
    label.Format("%u%%", percent);
  else
    label.Format("%s – %u%%", message.c_str(), percent);
  progress_bar.SetText(label.c_str());
}

void
ProgressWindow::SetRange(unsigned min_value, unsigned max_value) noexcept
{
  if (range_min != min_value || range_max != max_value)
    have_progress_position = false;
  range_min = min_value;
  range_max = max_value;
  progress_bar.SetRange(min_value, max_value);
  UpdateBarLabel();
}

void
ProgressWindow::SetStep(unsigned size) noexcept
{
  progress_bar.SetStep(size);
}

void
ProgressWindow::SetValue(unsigned value) noexcept
{
  AssertThread();

  have_progress_position = true;
  progress_bar.SetValue(value);
  UpdateBarLabel();
}

void
ProgressWindow::Step() noexcept
{
  have_progress_position = true;
  progress_bar.Step();
  UpdateBarLabel();
}

void
ProgressWindow::OnResize(PixelSize new_size) noexcept
{
  ContainerWindow::OnResize(new_size);

  UpdateLayout(GetClientRect());

  if (progress_bar.IsDefined())
    progress_bar.Move(progress_bar_position);

  Invalidate();
}

void
ProgressWindow::OnPaint(Canvas &canvas) noexcept
{
  canvas.Clear(background_color);

  logo.draw(canvas, logo_position, dark_mode);

  // Draw progress bar background
  canvas.Stretch(bottom_position.GetTopLeft(), bottom_position.GetSize(),
                 bitmap_progress_border);

  ContainerWindow::OnPaint(canvas);
}
