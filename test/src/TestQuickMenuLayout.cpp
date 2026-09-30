// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Menu/QuickMenuLayout.hpp"
#include "Menu/MenuData.hpp"
#include "TestUtil.hpp"

#include <vector>

using namespace QuickMenuLayout;

static void
TestChooseGrid()
{
  /* landscape: five columns of 150 pixels fit into 800 */
  Grid g = ChooseGrid(800, 400, 150, 40);
  ok1(g.columns == 5);
  ok1(g.rows == 10);

  /* an even number of columns is rounded down to an odd one */
  g = ChooseGrid(640, 400, 150, 40);
  ok1(g.columns == 3);

  /* portrait: three columns */
  g = ChooseGrid(480, 560, 150, 40);
  ok1(g.columns == 3);
  ok1(g.rows == 14);

  /* too small: still three columns and at least one row */
  g = ChooseGrid(300, 30, 150, 40);
  ok1(g.columns == 3);
  ok1(g.rows == 1);

  /* never more than seven columns */
  g = ChooseGrid(4000, 400, 150, 40);
  ok1(g.columns == 7);
}

/**
 * Every item sits at its location, counted row by row and page by
 * page; a missing location stays an empty cell.
 */
static void
TestLocations()
{
  const std::vector<unsigned> locations{1, 2, 5, 7, 16};
  const Grid grid{5, 3};
  const auto cells = Arrange(locations, grid);

  ok1(cells[0] == (Cell{0, 0, 0}));
  ok1(cells[1] == (Cell{0, 1, 0}));
  /* 3 and 4 are empty: 5 stays at the end of the first row */
  ok1(cells[2] == (Cell{0, 4, 0}));
  ok1(cells[3] == (Cell{0, 1, 1}));
  /* 15 cells per page: 16 opens the second page */
  ok1(cells[4] == (Cell{1, 0, 0}));
  ok1(CountPages(cells) == 2);

  /* the same locations in three columns */
  const auto narrow = Arrange(locations, Grid{3, 5});
  ok1(narrow[2] == (Cell{0, 1, 1}));
  ok1(narrow[3] == (Cell{0, 0, 2}));
  ok1(narrow[4] == (Cell{1, 0, 0}));
}

/**
 * Two items at the same location: the one listed first keeps it, the
 * other takes the next free one.
 */
static void
TestConflict()
{
  const std::vector<unsigned> locations{4, 4, 5};
  const auto cells = Arrange(locations, Grid{3, 5});
  ok1(cells[0] == (Cell{0, 0, 1}));
  ok1(cells[1] == (Cell{0, 1, 1}));
  ok1(cells[2] == (Cell{0, 2, 1}));
}

static void
TestShownRows()
{
  /* a short list shows at least five rows, so its buttons do not get
     huge */
  const std::vector<unsigned> few{1, 2, 3};
  const Grid grid{3, 10};
  const auto cells = Arrange(few, grid);
  ok1(CountShownRows(cells, 0, grid) == MIN_SHOWN_ROWS);

  /* a longer one shows the rows down to its lowest button */
  const std::vector<unsigned> more{1, 20};
  const auto more_cells = Arrange(more, grid);
  ok1(CountShownRows(more_cells, 0, grid) == 7);

  /* but never more than fit */
  const Grid low{3, 4};
  ok1(CountShownRows(Arrange(few, low), 0, low) == 4);
}

static void
TestNavigate()
{
  /* 3 x 3: index = location - 1 */
  std::vector<unsigned> locations;
  for (unsigned i = 1; i <= 9; ++i)
    locations.push_back(i);
  const auto cells = Arrange(locations, Grid{3, 5});

  /* from the middle (location 5) */
  ok1(Navigate(cells, 4, Direction::LEFT) == 3);
  ok1(Navigate(cells, 4, Direction::RIGHT) == 5);
  ok1(Navigate(cells, 4, Direction::UP) == 1);
  ok1(Navigate(cells, 4, Direction::DOWN) == 7);

  /* at the edge there is nowhere to go */
  ok1(Navigate(cells, 0, Direction::UP) == -1);

  /* an empty cell is jumped over: from 4, going right past the empty
     5 reaches 6 */
  const std::vector<unsigned> gap{4, 6};
  const auto gap_cells = Arrange(gap, Grid{3, 5});
  ok1(Navigate(gap_cells, 0, Direction::RIGHT) == 1);
}

/**
 * The placement of a menu item: a later file replaces the whole item,
 * placement included, and a location out of range is ignored.
 */
static void
TestMenuPlacement()
{
  Menu menu;
  menu.Clear();

  menu.Add("A", 3, 1, {8, 12, true});
  ok1(menu[3].portrait == 8);
  ok1(menu[3].landscape == 12);
  ok1(menu[3].center);

  menu.Add("B", 3, 2);
  ok1(menu[3].portrait == 0 && menu[3].landscape == 0 && !menu[3].center);

  menu.Add("C", 4, 3, {Menu::MAX_ITEMS, 5, false});
  ok1(menu[4].portrait == 0);
  ok1(menu[4].landscape == 5);
}

int
main()
{
  plan_tests(8 + 9 + 3 + 3 + 6 + 6);

  TestChooseGrid();
  TestLocations();
  TestConflict();
  TestShownRows();
  TestNavigate();
  TestMenuPlacement();

  return exit_status();
}
