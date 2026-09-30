// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Menu/QuickMenuLayout.hpp"
#include "Menu/MenuData.hpp"
#include "TestUtil.hpp"

#include <vector>

using namespace QuickMenuLayout;

static void
TestChooseColumns()
{
  /* five columns of 150 pixels fit into 800 */
  ok1(ChooseColumns(800, 150) == 5);

  /* an even number of columns is rounded down to an odd one */
  ok1(ChooseColumns(640, 150) == 3);

  /* too narrow: still three columns */
  ok1(ChooseColumns(300, 150) == 3);

  /* never more than seven */
  ok1(ChooseColumns(4000, 150) == 7);
}

static void
TestShownRows()
{
  /* a short list shows at least five rows, so its buttons do not get
     huge */
  ok1(CountShownRows(3, 400, 40) == MIN_SHOWN_ROWS);

  /* all rows if they fit */
  ok1(CountShownRows(8, 400, 40) == 8);

  /* never more than fit: the field scrolls */
  ok1(CountShownRows(20, 400, 40) == 10);
  ok1(CountShownRows(3, 120, 40) == 3);
}

/**
 * Every item sits at its location; a missing location stays an empty
 * cell.
 */
static void
TestLocations()
{
  const std::vector<Place> places{{0, 1}, {0, 2}, {0, 5}, {0, 7}};
  const auto cells = Arrange(places, 5);

  ok1(cells[0] == (Cell{0, 0}));
  ok1(cells[1] == (Cell{1, 0}));
  /* 3 and 4 are empty: 5 stays at the end of the first row */
  ok1(cells[2] == (Cell{4, 0}));
  ok1(cells[3] == (Cell{1, 1}));
  ok1(CountRows(cells) == 2);
}

/**
 * A picture of three columns stays the same in the middle of a wider
 * screen: the neighbours of the centre stay its neighbours.
 */
static void
TestCentred()
{
  const std::vector<Place> places{{3, 5}, {3, 4}, {3, 6}, {3, 2}, {3, 8}};

  const auto three = Arrange(places, 3);
  ok1(three[0] == (Cell{1, 1}));

  const auto seven = Arrange(places, 7);
  ok1(seven[0] == (Cell{3, 1}));
  ok1(seven[1] == (Cell{2, 1}));
  ok1(seven[2] == (Cell{4, 1}));
  ok1(seven[3] == (Cell{3, 0}));
  ok1(seven[4] == (Cell{3, 2}));
}

/**
 * A picture of seven columns on a screen of five: the middle stays,
 * the outer columns go to the first free cells below.
 */
static void
TestNarrower()
{
  /* row 0 of seven columns: 1 and 7 are outside, 2 to 6 fit */
  const std::vector<Place> places{{7, 1}, {7, 2}, {7, 4}, {7, 6}, {7, 7}};
  const auto cells = Arrange(places, 5);

  ok1(cells[1] == (Cell{0, 0}));
  ok1(cells[2] == (Cell{2, 0}));
  ok1(cells[3] == (Cell{4, 0}));
  ok1(cells[0] == (Cell{0, 1}));
  ok1(cells[4] == (Cell{1, 1}));
}

/**
 * Two items at the same location: the one listed first keeps it, the
 * other takes the next free one.
 */
static void
TestConflict()
{
  const std::vector<Place> places{{3, 4}, {3, 4}, {3, 5}};
  const auto cells = Arrange(places, 3);
  ok1(cells[0] == (Cell{0, 1}));
  ok1(cells[1] == (Cell{1, 1}));
  ok1(cells[2] == (Cell{2, 1}));
}

/**
 * Scrolling keeps one row of look-ahead: moving down, the focus stays
 * on the last but one row shown.
 */
static void
TestScroll()
{
  /* 10 rows, 5 shown */
  ok1(ScrollToShow(0, 5, 10, 3) == 0);
  ok1(ScrollToShow(0, 5, 10, 4) == 1);
  ok1(ScrollToShow(1, 5, 10, 5) == 2);
  ok1(ScrollToShow(1, 5, 10, 6) == 3);

  /* the last row needs no look-ahead */
  ok1(ScrollToShow(3, 5, 10, 9) == 5);

  /* moving up, the second row shown is the limit */
  ok1(ScrollToShow(5, 5, 10, 5) == 4);
  ok1(ScrollToShow(4, 5, 10, 0) == 0);

  /* everything fits: no scrolling */
  ok1(ScrollToShow(2, 5, 4, 3) == 0);

  /* the centre in the middle, as far as the field allows */
  ok1(ScrollToCenter(5, 10, 6) == 4);
  ok1(ScrollToCenter(5, 10, 1) == 0);
  ok1(ScrollToCenter(5, 10, 9) == 5);
}

static void
TestNavigate()
{
  /* 3 x 3: index = location - 1 */
  std::vector<Place> places;
  for (unsigned i = 1; i <= 9; ++i)
    places.push_back({3, i});
  const auto cells = Arrange(places, 3);

  /* from the middle (location 5) */
  ok1(Navigate(cells, 4, Direction::LEFT) == 3);
  ok1(Navigate(cells, 4, Direction::RIGHT) == 5);
  ok1(Navigate(cells, 4, Direction::UP) == 1);
  ok1(Navigate(cells, 4, Direction::DOWN) == 7);

  /* at the edge there is nowhere to go */
  ok1(Navigate(cells, 0, Direction::UP) == -1);

  /* an empty cell is jumped over: from 4, going right past the empty
     5 reaches 6 */
  const std::vector<Place> gap{{3, 4}, {3, 6}};
  const auto gap_cells = Arrange(gap, 3);
  ok1(Navigate(gap_cells, 0, Direction::RIGHT) == 1);
}

/**
 * The placement of a menu item: a later file replaces the whole item,
 * placement included, and values out of range are ignored.
 */
static void
TestMenuPlacement()
{
  Menu menu;
  menu.Clear();

  menu.Add("A", 3, 1, {8, 12, 0, 5, true});
  ok1(menu[3].portrait == 8);
  ok1(menu[3].landscape == 12);
  ok1(menu[3].landscape_columns == 5);
  ok1(menu[3].center);

  menu.Add("B", 3, 2);
  ok1(menu[3].portrait == 0 && menu[3].landscape == 0 &&
      menu[3].landscape_columns == 0 && !menu[3].center);

  menu.Add("C", 4, 3, {Menu::MAX_ITEMS, 5, 99, 7, false});
  ok1(menu[4].portrait == 0);
  ok1(menu[4].portrait_columns == 0);
  ok1(menu[4].landscape_columns == 7);
}

int
main()
{
  plan_tests(4 + 4 + 5 + 6 + 5 + 3 + 11 + 6 + 8);

  TestChooseColumns();
  TestShownRows();
  TestLocations();
  TestCentred();
  TestNarrower();
  TestConflict();
  TestScroll();
  TestNavigate();
  TestMenuPlacement();

  return exit_status();
}
