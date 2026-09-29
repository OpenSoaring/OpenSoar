// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <span>
#include <vector>

/**
 * The arrangement of the "dynamic" quick menu: a fixed block of
 * 3 x 5 buttons in the middle of the first page, which looks the same
 * in portrait and landscape, and the other buttons around it.
 *
 * The block holds the items with the locations 1 to 15, row by row
 * from the top left; location 8 is its centre.  The other items are
 * ranked by their location: the first page fills the cells around the
 * block with them, nearest to the centre first, and further pages hold
 * the rest in rows.
 *
 * This is plain geometry without any window, so the rules can be
 * tested on their own.
 */
namespace QuickMenuLayout {

static constexpr unsigned BLOCK_COLUMNS = 3;
static constexpr unsigned BLOCK_ROWS = 5;
static constexpr unsigned BLOCK_SIZE = BLOCK_COLUMNS * BLOCK_ROWS;

/** The location of the centre of the block. */
static constexpr unsigned CENTER_LOCATION = BLOCK_SIZE / 2 + 1;

/**
 * The number of columns and rows of a page; both are odd, so the
 * block can sit exactly in the middle.
 */
struct Grid {
  unsigned columns, rows;

  constexpr unsigned GetBlockLeft() const noexcept {
    return (columns - BLOCK_COLUMNS) / 2;
  }

  constexpr unsigned GetBlockTop() const noexcept {
    return (rows - BLOCK_ROWS) / 2;
  }

  constexpr bool IsInBlock(unsigned column, unsigned row) const noexcept {
    return column >= GetBlockLeft() &&
      column < GetBlockLeft() + BLOCK_COLUMNS &&
      row >= GetBlockTop() && row < GetBlockTop() + BLOCK_ROWS;
  }
};

struct Cell {
  unsigned page, column, row;

  constexpr bool operator==(const Cell &) const noexcept = default;
};

/**
 * How many columns and rows fit into an area, given the smallest
 * useful button size.  At least the block fits: if the area is too
 * small, the buttons get smaller instead.  At most seven columns, so
 * the buttons do not get too narrow on wide screens.
 */
[[gnu::const]]
Grid
ChooseGrid(unsigned width, unsigned height,
           unsigned min_column_width, unsigned min_row_height) noexcept;

/**
 * Place the items.
 *
 * @param locations the location of each item in the order in which
 * they are ranked (ascending locations); items missing from the menu
 * (other flight phase) are simply not in the list
 * @return one cell per item, in the same order
 */
std::vector<Cell>
Arrange(std::span<const unsigned> locations, Grid grid) noexcept;

[[gnu::pure]]
unsigned
CountPages(std::span<const Cell> cells) noexcept;

enum class Direction { LEFT, RIGHT, UP, DOWN };

/**
 * Which item the focus moves to from item @p from: the nearest item
 * on the same page in that direction, preferring one in the same row
 * or column.
 *
 * @return the index of that item, or -1 if there is none
 */
[[gnu::pure]]
int
Navigate(std::span<const Cell> cells, unsigned from,
         Direction direction) noexcept;

} // namespace QuickMenuLayout
