// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "QuickMenuLayout.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>

namespace QuickMenuLayout {

unsigned
ChooseColumns(unsigned width, unsigned min_column_width) noexcept
{
  unsigned n = min_column_width > 0
    ? std::min(width / min_column_width, MAX_COLUMNS)
    : MIN_COLUMNS;

  if (n <= MIN_COLUMNS)
    return MIN_COLUMNS;

  /* odd, so there is a middle column */
  return n % 2 == 0 ? n - 1 : n;
}

unsigned
CountShownRows(unsigned rows, unsigned height,
               unsigned min_row_height) noexcept
{
  const unsigned fit = min_row_height > 0
    ? std::max(height / min_row_height, 1u)
    : MIN_SHOWN_ROWS;

  return std::min(std::max(rows, MIN_SHOWN_ROWS), fit);
}

std::vector<Cell>
Arrange(std::span<const Place> places, unsigned columns) noexcept
{
  std::vector<Cell> result(places.size());

  struct Wanted {
    std::size_t item;

    /** the cell index (row * columns + column) */
    unsigned index;
  };

  std::vector<Wanted> fitting, outside;
  unsigned lowest_row = 0;

  for (std::size_t i = 0; i < places.size(); ++i) {
    const unsigned c = places[i].columns > 0 ? places[i].columns : columns;
    /* a location counts from 1; 0 would be before the first cell */
    const unsigned n = std::max(places[i].location, 1u) - 1;
    const unsigned row = n / c;

    /* centre the picture of c columns on the screen; the difference
       is rounded down, so an even one puts the extra column right */
    const int column = (int)(n % c) + ((int)columns - (int)c) / 2;

    if (column >= 0 && column < (int)columns) {
      fitting.push_back({i, row * columns + column});
      lowest_row = std::max(lowest_row, row);
    } else
      outside.push_back({i, row * columns});
  }

  /* the one listed first keeps a cell that two claim */
  auto by_index = [](const Wanted &a, const Wanted &b) noexcept {
    return a.index < b.index;
  };
  std::stable_sort(fitting.begin(), fitting.end(), by_index);
  std::stable_sort(outside.begin(), outside.end(), by_index);

  std::vector<bool> taken;
  auto take = [&](const Wanted &w, unsigned index) noexcept {
    while (index < taken.size() && taken[index])
      ++index;

    if (index >= taken.size())
      taken.resize(index + 1, false);
    taken[index] = true;

    result[w.item] = {index % columns, index / columns};
  };

  for (const auto &w : fitting)
    take(w, w.index);

  /* the columns that do not fit: below the picture, in the order of
     their locations */
  const unsigned below = fitting.empty() ? 0 : (lowest_row + 1) * columns;
  for (const auto &w : outside)
    take(w, below);

  return result;
}

unsigned
CountRows(std::span<const Cell> cells) noexcept
{
  unsigned n = 0;
  for (const auto &c : cells)
    n = std::max(n, c.row + 1);
  return n;
}

unsigned
ScrollToShow(unsigned top, unsigned shown, unsigned rows,
             unsigned row) noexcept
{
  if (rows <= shown)
    return 0;

  /* one row of look-ahead, if the field is tall enough for it */
  const unsigned margin = shown >= 3 ? 1 : 0;

  if (row < top + margin)
    top = row >= margin ? row - margin : 0;
  else if (row + margin >= top + shown)
    top = row + margin + 1 - shown;

  return std::min(top, rows - shown);
}

unsigned
ScrollToCenter(unsigned shown, unsigned rows, unsigned row) noexcept
{
  if (rows <= shown)
    return 0;

  const unsigned top = row >= shown / 2 ? row - shown / 2 : 0;
  return std::min(top, rows - shown);
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
    if (i == from)
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
