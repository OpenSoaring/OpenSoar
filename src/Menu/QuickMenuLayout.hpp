// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <span>
#include <vector>

/**
 * The arrangement of the quick menu in the style "OpenSoar": as many
 * columns as fit (more in landscape than in portrait), and every
 * button at its location, counted row by row and page by page.
 *
 * A location without a button stays an empty cell, so the other
 * buttons keep their places; that is what a pilot finds them by.
 * Because the number of columns differs between portrait and
 * landscape, an item can have a location of its own for each (see
 * MenuItem::portrait and MenuItem::landscape).
 *
 * This is plain geometry without any window, so the rules can be
 * tested on their own.
 */
namespace QuickMenuLayout {

static constexpr unsigned MIN_COLUMNS = 3;
static constexpr unsigned MAX_COLUMNS = 7;

/**
 * A page shows at least this many rows (if they fit), so that a
 * short list does not get huge buttons.
 */
static constexpr unsigned MIN_SHOWN_ROWS = 5;

/**
 * The number of columns and the number of rows that fit on a page.
 */
struct Grid {
  unsigned columns, rows;

  constexpr unsigned GetPageSize() const noexcept {
    return columns * rows;
  }
};

struct Cell {
  unsigned page, column, row;

  constexpr bool operator==(const Cell &) const noexcept = default;
};

/**
 * How many columns and rows fit into an area, given the smallest
 * useful button size.  The number of columns is odd, so there is a
 * middle column; it is at least #MIN_COLUMNS (the buttons get
 * narrower if the area is too small) and at most #MAX_COLUMNS, so the
 * buttons do not get too narrow on wide screens.
 */
[[gnu::const]]
Grid
ChooseGrid(unsigned width, unsigned height,
           unsigned min_column_width, unsigned min_row_height) noexcept;

/**
 * Place the items at their locations.
 *
 * @param locations the location (1 or more) of each item; items
 * missing from the menu are simply not in the list
 * @return one cell per item, in the same order
 *
 * If two items claim the same location, the one listed first keeps
 * it and the other goes to the next free location, so no button ever
 * covers another.
 */
std::vector<Cell>
Arrange(std::span<const unsigned> locations, Grid grid) noexcept;

[[gnu::pure]]
unsigned
CountPages(std::span<const Cell> cells) noexcept;

/**
 * How many rows the page @p page shows: the rows up to its lowest
 * button, but at least #MIN_SHOWN_ROWS and at most as many as fit.
 * The buttons get taller when a page shows fewer rows.
 */
[[gnu::pure]]
unsigned
CountShownRows(std::span<const Cell> cells, unsigned page,
               Grid grid) noexcept;

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
