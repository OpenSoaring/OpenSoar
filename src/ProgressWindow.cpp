// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ProgressWindow.hpp"
#include "Screen/Layout.hpp"
#include "Look/FontDescription.hpp"
#include "Resources.hpp"

#ifdef USE_WINUSER
#include "ui/canvas/AnyCanvas.hpp"
#else
#include "ui/canvas/Canvas.hpp"
#endif

ProgressWindow::ProgressWindow(ContainerWindow &parent) noexcept
  :background_color(COLOR_WHITE)
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
    text_height = canvas.GetFontHeight();
  }
#endif

  UpdateLayout(rc);

  // Initialize progress bar
  progress_bar.Create(*this, progress_bar_position);

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

  // Make progress bar height proportional to window height
  const unsigned progress_height = height / 20;
  const unsigned progress_horizontal_border = progress_height / 2;
  const unsigned progress_border_height = progress_height * 2;

  logo_position = rc;
  logo_position.bottom -= progress_border_height;

  message_position = rc;
  message_position.bottom -= progress_border_height + height / 48;
  message_position.top = message_position.bottom - text_height;

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

  if (text == nullptr)
    text = "";

  message = text;
  Invalidate(message_position);
}

void
ProgressWindow::SetRange(unsigned min_value, unsigned max_value) noexcept
{
  progress_bar.SetRange(min_value, max_value);
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

  progress_bar.SetValue(value);
}

void
ProgressWindow::Step() noexcept
{
  progress_bar.Step();
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

  logo.draw(canvas, logo_position);

  // Draw progress bar background
  canvas.Stretch(bottom_position.GetTopLeft(), bottom_position.GetSize(),
                 bitmap_progress_border);

#ifndef USE_WINUSER
  canvas.Select(font);
#endif
  canvas.SetBackgroundTransparent();
  canvas.SetTextColor(COLOR_BLACK);
  canvas.DrawText({(message_position.left + message_position.right
                    - (int)canvas.CalcTextWidth(message.c_str())) / 2,
      message_position.top},
                  message.c_str());

  ContainerWindow::OnPaint(canvas);
}
