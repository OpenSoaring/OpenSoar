// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <string_view>

struct PixelRect;
class Canvas;
class Color;

/**
 * @return the x coordinate where the filled part of the bar ends
 */
int
DrawSimpleProgressBar(Canvas &canvas, const PixelRect &r,
                      unsigned current_value,
                      unsigned min_value, unsigned max_value,
                      const Color *background_color = nullptr,
                      const Color *progress_color = nullptr) noexcept;

/**
 * @return the x coordinate where the filled part of the bar ends
 */
int
DrawRoundProgressBar(Canvas &canvas, const PixelRect &r,
                     unsigned current_value,
                     unsigned min_value, unsigned max_value,
                     const Color *background_color = nullptr) noexcept;

/**
 * Draw a text inside a progress bar.  The characters above the filled
 * part are drawn in #fill_text_color, the others in #text_color, so the
 * text stays readable while the bar grows underneath it.  The caller
 * selects the font.
 *
 * @param fill_end the x coordinate where the filled part ends, as
 * returned by DrawSimpleProgressBar() or DrawRoundProgressBar()
 * @param centered centre the text; otherwise it starts at the left edge
 */
void
DrawProgressBarText(Canvas &canvas, const PixelRect &r, int fill_end,
                    std::string_view text,
                    const Color &fill_text_color, const Color &text_color,
                    bool centered) noexcept;
