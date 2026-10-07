// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ProgressBar.hpp"
#include "ui/canvas/Features.hpp"
#include "ui/canvas/Canvas.hpp"
#include "ui/canvas/Font.hpp"
#include "Renderer/ProgressBarRenderer.hpp"
#include "Look/Colors.hpp"
#include "thread/Debug.hpp"

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
ProgressBar::SetText(const char *_text) noexcept
{
  AssertThread();

  if (_text == nullptr)
    _text = "";

  if (text == _text)
    return;

  text = _text;
  Invalidate();
}

#if defined(EYE_CANDY) && !defined(HAVE_CLIPPING)
/* when the Canvas is clipped, we can't render rounded corners,
   because the parent's background would not be left visible then */
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

  if (!text.empty()) {
    if (font != nullptr)
      canvas.Select(*font);
    DrawProgressBarText(canvas, canvas.GetRect(), fill_end, text,
                        COLOR_WHITE, COLOR_BLACK, true);
  }
}
