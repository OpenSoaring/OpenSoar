// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "SymbolRenderer.hpp"
#include "ui/canvas/Canvas.hpp"

#include <algorithm>

namespace {

[[gnu::pure]]
unsigned
MinDimension(const PixelRect &rc) noexcept
{
  return std::min(rc.GetWidth(), rc.GetHeight());
}

/** One fifth of the shorter rc side; used for arrows and bar symbols. */
[[gnu::pure]]
unsigned
DrawSize(const PixelRect &rc, unsigned max_draw_size) noexcept
{
  unsigned size = std::max(1u, MinDimension(rc) / 5);
  if (max_draw_size > 0)
    size = std::min(size, max_draw_size);

  return size;
}

/**
 * Horizontal bar half-extents (WithMargin size).
 * @param min_dim_for_width if non-zero, bar is 75% of this (menu icon)
 */
[[gnu::pure]]
PixelSize
BarMargin(unsigned draw_size, unsigned min_dim_for_width) noexcept
{
  const unsigned width = min_dim_for_width > 0
    ? std::max(1u, min_dim_for_width * 3 / 8)
    : draw_size;
  return {width, std::max(1u, draw_size / 3)};
}

void
DrawBarAt(Canvas &canvas, PixelPoint center, PixelSize margin) noexcept
{
  canvas.DrawRectangle(PixelRect{center}.WithMargin(margin));
}

} // namespace

void
SymbolRenderer::DrawArrow(Canvas &canvas, PixelRect rc,
                          Direction direction,
                          unsigned max_draw_size) noexcept
{
  assert(direction == UP || direction == DOWN ||
         direction == LEFT || direction == RIGHT);

  const unsigned size = DrawSize(rc, max_draw_size);
  const auto center = rc.GetCenter();
  BulkPixelPoint arrow[3];

  if (direction == LEFT || direction == RIGHT) {
    arrow[0].x = center.x + (direction == LEFT ? size : -size);
    arrow[0].y = center.y + size;
    arrow[1].x = center.x + (direction == LEFT ? -size : size);
    arrow[1].y = center.y;
    arrow[2].x = center.x + (direction == LEFT ? size : -size);
    arrow[2].y = center.y - size;
  } else if (direction == UP || direction == DOWN) {
    arrow[0].x = center.x + size;
    arrow[0].y = center.y + (direction == UP ? size : -size);
    arrow[1].x = center.x;
    arrow[1].y = center.y + (direction == UP ? -size : size);
    arrow[2].x = center.x - size;
    arrow[2].y = center.y + (direction == UP ? size : -size);
  }

  canvas.DrawTriangleFan(arrow, 3);
}

void
SymbolRenderer::DrawSign(Canvas &canvas, PixelRect rc, bool plus,
                         unsigned max_draw_size) noexcept
{
  if (MinDimension(rc) == 0)
    return;

  const unsigned draw_size = DrawSize(rc, max_draw_size);
  const auto center = rc.GetCenter();
  const PixelSize margin = BarMargin(draw_size, 0);

  DrawBarAt(canvas, center, margin);

  if (plus)
    DrawBarAt(canvas, center, {margin.height, margin.width});
}

void
SymbolRenderer::DrawHamburger(Canvas &canvas, PixelRect rc) noexcept
{
  const unsigned min_dim = MinDimension(rc);
  if (min_dim == 0)
    return;

  const unsigned draw_size = DrawSize(rc, 0);
  const auto center = rc.GetCenter();
  const PixelSize margin = BarMargin(draw_size, min_dim);
  const int step = int(2 * margin.height + std::max(1u, draw_size / 3));

  DrawBarAt(canvas, {center.x, center.y - step}, margin);
  DrawBarAt(canvas, center, margin);
  DrawBarAt(canvas, {center.x, center.y + step}, margin);
}

void
SymbolRenderer::DrawBolt(Canvas &canvas, PixelRect rc) noexcept
{
  const unsigned min_dim = MinDimension(rc);
  if (min_dim == 0)
    return;

  /* Material-style lightning bolt (filled zigzag), sized like the
     hamburger icon so it reads clearly on map overlay buttons. */
  const int s = int(std::max(1u, min_dim * 3 / 8));
  const auto c = rc.GetCenter();

  const BulkPixelPoint bolt[] = {
    {c.x + s * 2 / 10, c.y - s},
    {c.x - s * 5 / 10, c.y + s / 10},
    {c.x - s / 10, c.y + s / 10},
    {c.x - s * 2 / 10, c.y + s},
    {c.x + s * 5 / 10, c.y - s / 10},
    {c.x + s / 10, c.y - s / 10},
  };

  canvas.DrawPolygon(bolt, 6);
}

void
SymbolRenderer::DrawMedia(Canvas &canvas, PixelRect rc, MediaSymbol symbol,
                          unsigned max_draw_size) noexcept
{
  if (MinDimension(rc) == 0)
    return;

  const unsigned size = DrawSize(rc, max_draw_size);
  const auto center = rc.GetCenter();
  const int s = int(size);
  const unsigned bar_half = std::max(1u, size / 3);

  /* a filled triangle with its tip at x_tip, its base at x_base */
  const auto triangle = [&](int x_tip, int x_base) {
    const BulkPixelPoint t[] = {
      {x_base, center.y - s},
      {x_tip, center.y},
      {x_base, center.y + s},
    };
    canvas.DrawTriangleFan(t, 3);
  };

  /* a vertical bar centred at x */
  const auto bar = [&](int x) {
    DrawBarAt(canvas, {x, center.y}, {bar_half, size});
  };

  switch (symbol) {
  case MediaSymbol::PAUSE:
    bar(center.x - int(2 * bar_half));
    bar(center.x + int(2 * bar_half));
    break;

  case MediaSymbol::REWIND:
    triangle(center.x - s, center.x);
    triangle(center.x, center.x + s);
    break;

  case MediaSymbol::FORWARD:
    triangle(center.x + s, center.x);
    triangle(center.x, center.x - s);
    break;

  case MediaSymbol::SKIP_START:
    bar(center.x - s);
    triangle(center.x - s + int(2 * bar_half), center.x + s);
    break;

  case MediaSymbol::SKIP_END:
    triangle(center.x + s - int(2 * bar_half), center.x - s);
    bar(center.x + s);
    break;

  case MediaSymbol::TAKEOFF:
    {
      /* the departure symbol: an arrow lifting off the runway at 45
         degrees - shaft, head, and the ground line it leaves */
      const int o = std::max(1, s / 4);        /* shaft half width */
      const int hw = std::max(o + 1, s / 2);   /* head half width */

      const PixelPoint tail{center.x - s, center.y + s / 2};
      const PixelPoint neck{center.x + s / 4, center.y - s / 4};
      const PixelPoint tip{center.x + s, center.y - s};

      const BulkPixelPoint shaft[] = {
        {tail.x + o, tail.y + o},
        {neck.x + o, neck.y + o},
        {neck.x - o, neck.y - o},
        {tail.x - o, tail.y - o},
      };
      canvas.DrawTriangleFan(shaft, 4);

      const BulkPixelPoint head[] = {
        {neck.x + hw, neck.y + hw},
        {tip.x, tip.y},
        {neck.x - hw, neck.y - hw},
      };
      canvas.DrawTriangleFan(head, 3);

      /* the runway */
      DrawBarAt(canvas, {center.x, center.y + s},
                {size, std::max(1u, size / 4)});
    }
    break;
  }
}
