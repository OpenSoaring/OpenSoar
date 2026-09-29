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
 * The block holds the items with the locations 1 to 15.  They are
 * ranks, not cells: location 1 is the centre, where the focus starts,
 * and the next ones follow around it by importance (see
 * #BLOCK_CELLS):
 *
 *    8   6   9
 *   10   2  11
 *    3   1   4
 *   12   5  13
 *   14   7  15
 *
 * The other items are ranked by their location as well: the first page
 * fills the cells around the block with them, nearest to the centre
 * first, and further pages hold the rest in rows.
 *
 * This is plain geometry without any window, so the rules can be
 * tested on their own.
 */
namespace QuickMenuLayout {

static constexpr unsigned BLOCK_COLUMNS = 3;
static constexpr unsigned BLOCK_ROWS = 5;
static constexpr unsigned BLOCK_SIZE = BLOCK_COLUMNS * BLOCK_ROWS;

/** The location of the centre of the block. */
static constexpr unsigned CENTER_LOCATION = 1;

/**
 * The cell in the block (column, row) of the locations 1 to 15: the
 * centre, then above, left, right and below it, then two above and
 * two below, the upper corners, the neighbours of the row above, those
 * of the row below and last the lower corners.
 */
static constexpr struct { unsigned column, row; } BLOCK_CELLS[BLOCK_SIZE] = {
  {1, 2},
  {1, 1}, {0, 2}, {2, 2}, {1, 3},
  {1, 0}, {1, 4},
  {0, 0}, {2, 0},
  {0, 1}, {2, 1},
  {0, 3}, {2, 3},
  {0, 4}, {2, 4},
};

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
 * @param locations the location (rank) of each item; items missing
 * from the menu are simply not in the list
 * @return one cell per item, in the same order
 */
std::vector<Cell>
Arrange(std::span<const unsigned> locations, Grid grid) noexcept;

/**
 * Convert the locations of a list that was written for the three
 * columns of the XCSoar style into ranks for Arrange(): the 3 x 5
 * cells around the location @p center keep their places and form the
 * block, @p center its centre; the other rows follow outwards, nearer
 * rows first.  So the picture the author of such a list had in mind
 * stays the same in the middle of the screen.
 */
std::vector<unsigned>
RanksFromColumns(std::span<const unsigned> locations,
                 unsigned center) noexcept;

/**
 * Place the items of a list for the three columns of the XCSoar style
 * on a grid of three columns (portrait) exactly as they were, just
 * shifted so that @p center is the centre of the block.  What does
 * not fit above or below goes to the following pages.
 */
std::vector<Cell>
ArrangeColumns(std::span<const unsigned> locations, unsigned center,
               Grid grid) noexcept;

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
