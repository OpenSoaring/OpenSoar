// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "FrequencyListRenderer.hpp"
#include "TextRowRenderer.hpp"
#include "ui/canvas/Canvas.hpp"
#include "util/StaticString.hxx"
#include "util/Macros.hpp"

typedef StaticString<256u> Buffer;

void
FrequencyListRenderer::Draw(Canvas &canvas, PixelRect rc,
                           const RadioChannel &channel,
                           const TextRowRenderer &row_renderer)
{
  // Draw name and frequency
  row_renderer.DrawTextRow(canvas, rc, channel.name.c_str());

  if (channel.radio_frequency.IsDefined()) {
  	StaticString<30> buffer;
  	char radio[20];
    channel.radio_frequency.Format(radio, ARRAY_SIZE(radio));
    buffer.Format("%s MHz", radio);
    row_renderer.DrawRightColumn(canvas, rc, buffer);
  }
}
