// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "ProgressBarRenderer.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Look/Colors.hpp"
#include "Asset.hpp"
#include "util/UTF8.hpp"

#include <algorithm>

static constexpr unsigned
CalcProgressBarPosition(unsigned current_value,
                        unsigned min_value, unsigned max_value,
                        unsigned width) noexcept
{
  if (min_value >= max_value)
    return 0;

  const unsigned value = std::clamp(current_value, min_value, max_value);
  return (value - min_value) * width / (max_value - min_value);
}

int
DrawSimpleProgressBar(Canvas &canvas, const PixelRect &r,
                      unsigned current_value,
                      unsigned min_value, unsigned max_value,
                      const Color *background_color,
                      const Color *progress_color) noexcept
{
  const int position =
    CalcProgressBarPosition(current_value, min_value, max_value,
                            r.GetWidth());

  auto a = r, b = r;
  a.right = b.left = a.left + position;

  const Color fill_color = IsDithered()
    ? COLOR_BLACK
    : progress_color != nullptr
      ? HasColors() ? *progress_color : COLOR_BLACK
      : COLOR_GREEN;
  canvas.DrawFilledRectangle(a, fill_color);
  canvas.DrawFilledRectangle(b,
                             background_color != nullptr
                             ? *background_color : COLOR_WHITE);
  return a.right;
}

int
DrawRoundProgressBar(Canvas &canvas, const PixelRect &r,
                     unsigned current_value,
                     unsigned min_value, unsigned max_value,
                     const Color *background_color) noexcept
{
  const unsigned position =
    CalcProgressBarPosition(current_value, min_value, max_value,
                            r.GetWidth());

  canvas.SelectNullPen();
  if (background_color != nullptr) {
    Brush bg_brush(*background_color);
    canvas.Select(bg_brush);
  } else {
    canvas.SelectWhiteBrush();
  }
  canvas.DrawRoundRectangle(r, PixelSize{r.GetHeight()});

  /* dark enough for the white text that DrawProgressBarText() puts on
     the filled part */
  Brush progress_brush(IsDithered() ? COLOR_BLACK : COLOR_XCSOAR);
  canvas.Select(progress_brush);
  unsigned margin = r.GetHeight() / 9;
  unsigned top, bottom;
  if (position <= r.GetHeight() - 2 * margin) {
    // Use a centered "circle" for small position values. This keeps the progress
    // bar inside the background.
    unsigned center_y = r.GetHeight() / 2;
    top = center_y - position / 2;
    bottom = center_y + position / 2;
  } else {
    top = margin;
    bottom = r.GetHeight() - margin;
  }
  canvas.DrawRoundRectangle(PixelRect(margin, top, margin + position, bottom),
                            PixelSize{r.GetHeight()});
  return r.left + (int)(margin + position);
}

void
DrawProgressBarText(Canvas &canvas, const PixelRect &r, int fill_end,
                    std::string_view text,
                    const Color &fill_text_color, const Color &text_color,
                    bool centered) noexcept
{
  if (text.empty())
    return;

  const int height = r.GetHeight();
  const int text_height = canvas.GetFontHeight();
  const int padding = std::max((height - text_height) / 2, 0);
  const int text_width = canvas.CalcTextWidth(text);

  int x = r.left + padding;
  if (centered)
    x = std::max(x, r.left + (int(r.GetWidth()) - text_width) / 2);
  const int y = r.top + (height - text_height) / 2;

  /* only the characters lying completely on the filled part get the
     colour for the filled part: a light character half on the light
     background would be lost, a dark one half on the filled part stays
     readable; a split inside a character would need clipping, which not
     every canvas offers */
  std::size_t split = 0;
  while (split < text.size()) {
    std::size_t next = split +
      std::min<std::size_t>(SequenceLengthUTF8(text[split]),
                            text.size() - split);
    if (next == split)
      ++next;
    const int right = x + (int)canvas.CalcTextWidth(text.substr(0, next));
    if (right > fill_end)
      break;
    split = next;
  }

  canvas.SetBackgroundTransparent();
  if (split > 0) {
    canvas.SetTextColor(fill_text_color);
    canvas.DrawClippedText({x, y}, r, text.substr(0, split));
  }
  if (split < text.size()) {
    canvas.SetTextColor(text_color);
    canvas.DrawClippedText({x + (int)canvas.CalcTextWidth(text.substr(0, split)), y},
                           r, text.substr(split));
  }
}
