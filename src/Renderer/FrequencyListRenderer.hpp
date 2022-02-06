// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "RadioFrequency.hpp"
#include <string>

class Canvas;
class TextRowRenderer;
struct PixelRect;

namespace FrequencyListRenderer
{
  struct RadioChannel {
	  std::string name;
	  RadioFrequency radio_frequency;
  };

  void Draw(Canvas &canvas, PixelRect rc, const RadioChannel &channel,
            const TextRowRenderer &row_renderer);
}

