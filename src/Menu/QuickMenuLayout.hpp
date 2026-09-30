// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <span>
#include <vector>

/**
 * The arrangement of the quick menu in the style "OpenSoar": one
 * field of buttons that grows downwards and scrolls, as many columns
 * as fit, and every button at its location.
 *
 * A location is counted row by row from the top left in a given
 * number of columns (see #Place).  On a wider screen that picture is
 * centred, so the buttons keep their neighbours left, right, above
 * and below; on a narrower one the outer columns do not fit and their
 * buttons move to the first free cells below.  A location without a
 * button stays an empty cell, so the other buttons keep their places;
 * that is what a pilot finds them by.
 *
 * This is plain geometry without any window, so the rules can be
 * tested on their own.
 */
namespace QuickMenuLayout {

static constexpr unsigned MIN_COLUMNS = 3;
static constexpr unsigned MAX_COLUMNS = 7;

/**
 * The field shows at least this many rows (if they fit), so that a
 * short list does not get huge buttons.
 */
static constexpr unsigned MIN_SHOWN_ROWS = 5;

/**
 * Where a list puts a button: a location (1 or more) counted in
 * @a columns columns; 0 columns means those of the screen.
 */
struct Place {
  unsigned columns, location;
};

struct Cell {
  unsigned column, row;

  constexpr bool operator==(const Cell &) const noexcept = default;
};

/**
 * How many columns fit side by side, given the narrowest useful
 * button.  The number is odd, so there is a middle column; it is at
 * least #MIN_COLUMNS (the buttons get narrower if the area is too
 * small) and at most #MAX_COLUMNS, so the buttons do not get too
 * narrow on wide screens.
 */
[[gnu::const]]
unsigned
ChooseColumns(unsigned width, unsigned min_column_width) noexcept;

/**
 * How many rows the field shows: all rows if they fit, at least
 * #MIN_SHOWN_ROWS (if they fit), and never more than fit; if the
 * list has more rows, the field scrolls.
 */
[[gnu::const]]
unsigned
CountShownRows(unsigned rows, unsigned height,
               unsigned min_row_height) noexcept;

/**
 * Place the items.
 *
 * @param places the place of each item; items missing from the menu
 * are simply not in the list
 * @param columns the number of columns of the screen
 * @return one cell per item, in the same order
 *
 * If two items claim the same cell, the one listed first keeps it and
 * the other goes to the next free cell, so no button ever covers
 * another.
 */
std::vector<Cell>
Arrange(std::span<const Place> places, unsigned columns) noexcept;

/**
 * The number of rows the items take (up to the lowest one).
 */
[[gnu::pure]]
unsigned
CountRows(std::span<const Cell> cells) noexcept;

/**
 * The first row to show so that @p row is visible with one more row
 * beyond it (if there is one): moving down, the focus stays on the
 * last but one row and the field scrolls by one row at a time.
 *
 * @param top the first row shown now
 * @param shown the number of rows shown
 * @param rows the number of rows of the field
 */
[[gnu::const]]
unsigned
ScrollToShow(unsigned top, unsigned shown, unsigned rows,
             unsigned row) noexcept;

/**
 * The first row to show so that @p row is in the middle, as far as
 * the field allows.
 */
[[gnu::const]]
unsigned
ScrollToCenter(unsigned shown, unsigned rows, unsigned row) noexcept;

enum class Direction { LEFT, RIGHT, UP, DOWN };

/**
 * Which item the focus moves to from item @p from: the nearest item
 * in that direction, preferring one in the same row or column.
 *
 * @return the index of that item, or -1 if there is none
 */
[[gnu::pure]]
int
Navigate(std::span<const Cell> cells, unsigned from,
         Direction direction) noexcept;

} // namespace QuickMenuLayout
