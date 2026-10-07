// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ProgressBar.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Renderer/ProgressBarRenderer.hpp"
#include "thread/Debug.hpp"
#include "Look/FontDescription.hpp"
#include "Look/Colors.hpp"
#include "Screen/Layout.hpp"

#include <algorithm>

void
ProgressBar::SetRange(unsigned min_value, unsigned max_value)
{
  AssertThread();

  this->min_value = min_value;
  this->max_value = max_value;
  value = 0;
  step_size = 1;
  Invalidate();
}

void
ProgressBar::SetValue(unsigned value)
{
  AssertThread();

  if (value == this->value)
    return;

  this->value = value;
  Invalidate();
}

void
ProgressBar::SetStep(unsigned size)
{
  AssertThread();

  step_size = size;
  Invalidate();
}

void
ProgressBar::Step()
{
  AssertThread();

  value += step_size;
  Invalidate();
}

void
ProgressBar::SetText(const char *text) noexcept
{
  AssertThread();

  if (text == nullptr)
    text = "";

  if (caption == text)
    return;

  caption = text;
  if (!font.IsDefined())
    font.Load(FontDescription(Layout::FontScale(10)));
  Invalidate();
}

#ifdef EYE_CANDY
/* Rounded corners leave the parent's background visible in the
   corner pixels. */
#define ROUND_PROGRESS_BAR
#endif

void
ProgressBar::OnPaint(Canvas &canvas) noexcept
{
#ifdef ROUND_PROGRESS_BAR
  const int fill_end =
    DrawRoundProgressBar(canvas, canvas.GetRect(), value, min_value, max_value);
#else
  const int fill_end =
    DrawSimpleProgressBar(canvas, canvas.GetRect(), value, min_value, max_value);
#endif

  if (caption.empty() || !font.IsDefined())
    return;

  canvas.Select(font);
  canvas.SetBackgroundTransparent();

  const PixelSize size = canvas.CalcTextSize(caption);
  const int x = std::max((int(canvas.GetWidth()) - int(size.width)) / 2,
                         int(size.height) / 2);
  const int y = (int(canvas.GetHeight()) - int(size.height)) / 2;

  canvas.SetTextColor(COLOR_BLACK);
  canvas.DrawClippedText({x, y}, canvas.GetRect(), std::string_view{caption});

  /* Redraw the part that sits on the filled bar in white, so the text
     changes from dark to light where the bar has grown underneath it;
     the fill is dark enough for that, in colour and on e-paper. */
  if (x >= fill_end)
    return;

  canvas.SetTextColor(COLOR_WHITE);
  canvas.DrawClippedText({x, y}, unsigned(fill_end - x),
                         std::string_view{caption});
}
