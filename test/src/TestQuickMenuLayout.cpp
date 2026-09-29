// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Menu/QuickMenuLayout.hpp"
#include "TestUtil.hpp"

#include <vector>

using namespace QuickMenuLayout;

static void
TestChooseGrid()
{
  /* landscape: five columns of 150 pixels fit into 800 */
  Grid g = ChooseGrid(800, 400, 150, 40);
  ok1(g.columns == 5);
  ok1(g.rows == 9);

  /* an even number of columns is rounded down to an odd one */
  g = ChooseGrid(640, 400, 150, 40);
  ok1(g.columns == 3);

  /* portrait: at least the block */
  g = ChooseGrid(480, 560, 150, 40);
  ok1(g.columns == 3);
  ok1(g.rows == 13);

  /* too small for the block: the block still fits, smaller */
  g = ChooseGrid(300, 100, 150, 40);
  ok1(g.columns == 3);
  ok1(g.rows == 5);

  /* never more than seven columns */
  g = ChooseGrid(4000, 400, 150, 40);
  ok1(g.columns == 7);
}

/**
 * The block is the same in both orientations; only the other items
 * move.
 */
static void
TestBlockStaysTheSame()
{
  std::vector<unsigned> locations;
  for (unsigned i = 1; i <= 20; ++i)
    locations.push_back(i);

  const Grid landscape{5, 7};
  const Grid portrait{3, 9};

  const auto l = Arrange(locations, landscape);
  const auto p = Arrange(locations, portrait);

  bool same = true;
  for (unsigned i = 0; i < BLOCK_SIZE; ++i) {
    const unsigned lc = l[i].column - landscape.GetBlockLeft();
    const unsigned lr = l[i].row - landscape.GetBlockTop();
    const unsigned pc = p[i].column - portrait.GetBlockLeft();
    const unsigned pr = p[i].row - portrait.GetBlockTop();
    if (l[i].page != 0 || p[i].page != 0 || lc != pc || lr != pr)
      same = false;
  }
  ok1(same);

  /* the 15 locations of the block cover its 15 cells, each once */
  bool used[BLOCK_COLUMNS][BLOCK_ROWS] = {};
  unsigned distinct = 0;
  for (const auto &c : BLOCK_CELLS)
    if (c.column < BLOCK_COLUMNS && c.row < BLOCK_ROWS &&
        !used[c.column][c.row]) {
      used[c.column][c.row] = true;
      ++distinct;
    }
  ok1(distinct == BLOCK_SIZE);

  /* the centre location is in the middle of the page */
  ok1(l[CENTER_LOCATION - 1] == (Cell{0, 2, 3}));
  ok1(p[CENTER_LOCATION - 1] == (Cell{0, 1, 4}));

  /* landscape: the first item after the block goes to the left of
     the centre row, the next one to the right */
  ok1(l[15] == (Cell{0, 0, 3}));
  ok1(l[16] == (Cell{0, 4, 3}));

  /* portrait: above and below the block */
  ok1(p[15] == (Cell{0, 1, 1}));
  ok1(p[16] == (Cell{0, 1, 7}));
}

static void
TestGapsAndPages()
{
  /* the centre (location 1) is missing: its cell stays empty, the
     others keep their places left and right of it */
  const std::vector<unsigned> locations{2, 3, 4, 15, 30};
  const Grid grid{3, 5};
  const auto cells = Arrange(locations, grid);
  ok1(cells[1] == (Cell{0, 0, 2}));
  ok1(cells[2] == (Cell{0, 2, 2}));

  /* a 3 x 5 page has no room around the block: page 2, top left */
  ok1(cells[4] == (Cell{1, 0, 0}));
  ok1(CountPages(cells) == 2);
}

static void
TestNavigate()
{
  std::vector<unsigned> locations;
  for (unsigned i = 1; i <= 15; ++i)
    locations.push_back(i);
  /* around the centre (location 1): 2 above, 3 left, 4 right, 5
     below; index = location - 1 */
  const auto cells = Arrange(locations, Grid{5, 7});
  ok1(Navigate(cells, 0, Direction::LEFT) == 2);
  ok1(Navigate(cells, 0, Direction::RIGHT) == 3);
  ok1(Navigate(cells, 0, Direction::UP) == 1);
  ok1(Navigate(cells, 0, Direction::DOWN) == 4);

  /* at the edge there is nowhere to go: location 8 is the upper left
     corner */
  ok1(Navigate(cells, 7, Direction::UP) == -1);

  /* a gap is jumped over: without the centre, going right from 3
     reaches 4 */
  const std::vector<unsigned> gap{3, 4};
  const auto gap_cells = Arrange(gap, Grid{5, 7});
  ok1(Navigate(gap_cells, 0, Direction::RIGHT) == 1);
}

/**
 * A list for the three columns of the XCSoar style keeps the picture
 * around its centre.
 */
static void
TestRanksFromColumns()
{
  /* the rows 5 to 9 of three columns around location 20 */
  const std::vector<unsigned> locations{
    13, 14, 15,
    16, 17, 18,
    19, 20, 21,
    22, 23, 24,
    25, 26, 27,
    10, 11, 12, 28, 29, 30, 1,
  };
  const auto ranks = RanksFromColumns(locations, 20);

  const std::vector<unsigned> expected{
    8, 6, 9,
    10, 2, 11,
    3, 1, 4,
    12, 5, 13,
    14, 7, 15,
    /* the row above first (middle column first), then the one
       below, then the far rows */
    17, 16, 18, 20, 19, 21, 22,
  };
  ok1(ranks == expected);

  /* so the cells around the centre are the same as in three columns */
  const auto cells = Arrange(ranks, Grid{3, 5});
  ok1(cells[0] == (Cell{0, 0, 0}));
  ok1(cells[7] == (Cell{0, 1, 2}));
  ok1(cells[14] == (Cell{0, 2, 4}));
}

static void
TestArrangeColumns()
{
  /* 3 x 7 cells: the centre 20 (row 6 of three columns) comes into
     row 3, so rows 4 to 10 fit (locations 10 to 30) */
  const std::vector<unsigned> locations{1, 10, 20, 21, 30, 31};
  const auto cells = ArrangeColumns(locations, 20, Grid{3, 7});
  ok1(cells[2] == (Cell{0, 1, 3}));
  ok1(cells[3] == (Cell{0, 2, 3}));
  ok1(cells[1] == (Cell{0, 0, 0}));
  ok1(cells[4] == (Cell{0, 2, 6}));

  /* above and below the page: the next page, by location */
  ok1(cells[0] == (Cell{1, 0, 0}));
  ok1(cells[5] == (Cell{1, 1, 0}));
}

int
main()
{
  plan_tests(8 + 8 + 4 + 6 + 4 + 6);

  TestChooseGrid();
  TestBlockStaysTheSame();
  TestGapsAndPages();
  TestNavigate();
  TestRanksFromColumns();
  TestArrangeColumns();

  return exit_status();
}
