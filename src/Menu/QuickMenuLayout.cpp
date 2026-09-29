// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "QuickMenuLayout.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>

namespace QuickMenuLayout {

static constexpr unsigned MAX_COLUMNS = 7;

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
    : BLOCK_COLUMNS;
  const unsigned rows = min_row_height > 0
    ? height / min_row_height
    : BLOCK_ROWS;

  return {
    OddAtLeast(std::min(columns, MAX_COLUMNS), BLOCK_COLUMNS),
    OddAtLeast(rows, BLOCK_ROWS),
  };
}

/**
 * The cells of the first page outside the block, nearest to the
 * centre of the block first.  Distance ties go to the upper cell,
 * then to the left one, so the order does not depend on the sort
 * algorithm.
 */
static std::vector<Cell>
FreeCellsByDistance(Grid grid) noexcept
{
  std::vector<Cell> cells;
  for (unsigned row = 0; row < grid.rows; ++row)
    for (unsigned column = 0; column < grid.columns; ++column)
      if (!grid.IsInBlock(column, row))
        cells.push_back({0, column, row});

  const int center_column = grid.columns / 2;
  const int center_row = grid.rows / 2;

  auto distance = [=](const Cell &c) noexcept {
    const int dx = (int)c.column - center_column;
    const int dy = (int)c.row - center_row;
    return dx * dx + dy * dy;
  };

  std::sort(cells.begin(), cells.end(),
            [&](const Cell &a, const Cell &b) noexcept {
              const int da = distance(a), db = distance(b);
              if (da != db)
                return da < db;
              if (a.row != b.row)
                return a.row < b.row;
              return a.column < b.column;
            });

  return cells;
}

std::vector<Cell>
Arrange(std::span<const unsigned> locations, Grid grid) noexcept
{
  std::vector<Cell> result(locations.size());

  const auto free_cells = FreeCellsByDistance(grid);
  const unsigned page_size = grid.columns * grid.rows;

  /* the block by location, the others in the order of their rank */
  std::vector<std::size_t> order(locations.size());
  for (std::size_t i = 0; i < order.size(); ++i)
    order[i] = i;
  std::stable_sort(order.begin(), order.end(),
                   [&](std::size_t a, std::size_t b) noexcept {
                     return locations[a] < locations[b];
                   });

  unsigned next_free = 0, overflow = 0;

  for (const std::size_t i : order) {
    const unsigned location = locations[i];

    if (location >= 1 && location <= BLOCK_SIZE) {
      const auto &cell = BLOCK_CELLS[location - 1];
      result[i] = {
        0,
        grid.GetBlockLeft() + cell.column,
        grid.GetBlockTop() + cell.row,
      };
    } else if (next_free < free_cells.size()) {
      result[i] = free_cells[next_free++];
    } else {
      /* the following pages have no block, just rows */
      const unsigned page = 1 + overflow / page_size;
      const unsigned position = overflow % page_size;
      result[i] = {page, position % grid.columns, position / grid.columns};
      ++overflow;
    }
  }

  return result;
}

std::vector<unsigned>
RanksFromColumns(std::span<const unsigned> locations,
                 unsigned center) noexcept
{
  auto column_of = [](unsigned location) noexcept {
    return (int)((location - 1) % BLOCK_COLUMNS);
  };
  auto row_of = [](unsigned location) noexcept {
    return (int)((location - 1) / BLOCK_COLUMNS);
  };

  const int center_column = column_of(center);
  const int center_row = row_of(center);
  const int block_half_height = BLOCK_ROWS / 2;

  std::vector<unsigned> ranks(locations.size(), 0);

  /* the rows outside the block: nearer rows first, the upper one
     before the lower one, the middle column before the sides */
  struct Outside {
    std::size_t index;
    int distance, dy, dx;
  };
  std::vector<Outside> outside;

  for (std::size_t i = 0; i < locations.size(); ++i) {
    if (locations[i] == 0) {
      outside.push_back({i, INT_MAX, 0, 0});
      continue;
    }

    const int dx = column_of(locations[i]) - center_column;
    const int dy = row_of(locations[i]) - center_row;

    if (std::abs(dy) <= block_half_height) {
      const unsigned column = dx + 1, row = dy + block_half_height;
      for (unsigned rank = 0; rank < BLOCK_SIZE; ++rank)
        if (BLOCK_CELLS[rank].column == column &&
            BLOCK_CELLS[rank].row == row)
          ranks[i] = rank + 1;
    } else {
      outside.push_back({i, std::abs(dy), dy, dx});
    }
  }

  std::stable_sort(outside.begin(), outside.end(),
                   [](const Outside &a, const Outside &b) noexcept {
                     if (a.distance != b.distance)
                       return a.distance < b.distance;
                     if (a.dy != b.dy)
                       return a.dy < b.dy;
                     if (std::abs(a.dx) != std::abs(b.dx))
                       return std::abs(a.dx) < std::abs(b.dx);
                     return a.dx < b.dx;
                   });

  unsigned next = BLOCK_SIZE + 1;
  for (const auto &o : outside)
    ranks[o.index] = next++;

  return ranks;
}

std::vector<Cell>
ArrangeColumns(std::span<const unsigned> locations, unsigned center,
               Grid grid) noexcept
{
  std::vector<Cell> result(locations.size());

  const int center_row = (int)((center - 1) / BLOCK_COLUMNS);
  const int top = (int)grid.GetBlockTop() + (int)BLOCK_ROWS / 2 - center_row;
  const unsigned page_size = grid.columns * grid.rows;

  /* those that do not fit, in the order of their location */
  std::vector<std::size_t> overflow;

  for (std::size_t i = 0; i < locations.size(); ++i) {
    const unsigned location = locations[i];
    const int row = location > 0
      ? top + (int)((location - 1) / BLOCK_COLUMNS)
      : -1;

    if (row >= 0 && row < (int)grid.rows)
      result[i] = {0, (location - 1) % BLOCK_COLUMNS, (unsigned)row};
    else
      overflow.push_back(i);
  }

  std::stable_sort(overflow.begin(), overflow.end(),
                   [&](std::size_t a, std::size_t b) noexcept {
                     return locations[a] < locations[b];
                   });

  unsigned n = 0;
  for (const std::size_t i : overflow) {
    const unsigned page = 1 + n / page_size;
    const unsigned position = n % page_size;
    result[i] = {page, position % grid.columns, position / grid.columns};
    ++n;
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
