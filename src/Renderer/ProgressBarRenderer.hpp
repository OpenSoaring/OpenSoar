// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

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
