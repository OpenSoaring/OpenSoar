// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "QuickMenuLayout.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>

namespace QuickMenuLayout {

/**
 * The largest odd number not above @p n, but at least @p minimum.
 */
static constexpr unsigned
OddAtLeast(unsigned n, unsigned minimum) noexcept
{
  if (n <= minimum)
    return minimum;

  return n % 2 == 0 ? n - 1 : n;
}

Grid
ChooseGrid(unsigned width, unsigned height,
           unsigned min_column_width, unsigned min_row_height) noexcept
{
  const unsigned columns = min_column_width > 0
    ? width / min_column_width
    : MIN_COLUMNS;
  const unsigned rows = min_row_height > 0
    ? height / min_row_height
    : MIN_SHOWN_ROWS;

  return {
    OddAtLeast(std::min(columns, MAX_COLUMNS), MIN_COLUMNS),
    std::max(rows, 1u),
  };
}

std::vector<Cell>
Arrange(std::span<const unsigned> locations, Grid grid) noexcept
{
  std::vector<Cell> result(locations.size());

  const unsigned page_size = grid.GetPageSize();

  /* the lower locations first; for the same location the item listed
     first wins */
  std::vector<std::size_t> order(locations.size());
  for (std::size_t i = 0; i < order.size(); ++i)
    order[i] = i;
  std::stable_sort(order.begin(), order.end(),
                   [&](std::size_t a, std::size_t b) noexcept {
                     return locations[a] < locations[b];
                   });

  std::vector<bool> taken;

  for (const std::size_t i : order) {
    /* a location counts from 1; 0 would be before the first cell */
    unsigned index = std::max(locations[i], 1u) - 1;
    while (index < taken.size() && taken[index])
      ++index;

    if (index >= taken.size())
      taken.resize(index + 1, false);
    taken[index] = true;

    const unsigned position = index % page_size;
    result[i] = {
      index / page_size,
      position % grid.columns,
      position / grid.columns,
    };
  }

  return result;
}

unsigned
CountPages(std::span<const Cell> cells) noexcept
{
  unsigned n = 1;
  for (const auto &c : cells)
    n = std::max(n, c.page + 1);
  return n;
}

unsigned
CountShownRows(std::span<const Cell> cells, unsigned page,
               Grid grid) noexcept
{
  unsigned used = 0;
  for (const auto &c : cells)
    if (c.page == page)
      used = std::max(used, c.row + 1);

  return std::max(used, std::min(MIN_SHOWN_ROWS, grid.rows));
}

int
Navigate(std::span<const Cell> cells, unsigned from,
         Direction direction) noexcept
{
  if (from >= cells.size())
    return -1;

  const Cell &origin = cells[from];

  int best = -1;
  int best_score = INT_MAX;

  for (std::size_t i = 0; i < cells.size(); ++i) {
    const Cell &c = cells[i];
    if (i == from || c.page != origin.page)
      continue;

    const int dx = (int)c.column - (int)origin.column;
    const int dy = (int)c.row - (int)origin.row;

    int along, across;
    switch (direction) {
    case Direction::LEFT:
      along = -dx;
      across = dy;
      break;

    case Direction::RIGHT:
      along = dx;
      across = dy;
      break;

    case Direction::UP:
      along = -dy;
      across = dx;
      break;

    case Direction::DOWN:
      along = dy;
      across = dx;
      break;

    default:
      return -1;
    }

    if (along <= 0)
      continue;

    /* a step sideways counts more than a step ahead, so an item in
       the same row or column wins over a closer one beside it */
    const int score = along + 3 * std::abs(across);
    if (score < best_score) {
      best_score = score;
      best = (int)i;
    }
  }

  return best;
}

} // namespace QuickMenuLayout
