// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LogbookComputer.hpp"
#include "Engine/Contest/LogbookStatistics.hpp"

LogbookComputer::LogbookComputer(const Trace &trace_full,
                                 const Trace &trace_triangle) noexcept
  :free(trace_full),
   dmst_quad(trace_full),
   /* the log book records what was flown, so the triangle is not
      assumed to be closed */
   dmst_triangle(trace_triangle, false),
   dmst_or(trace_full)
{
  Reset();
}

void
LogbookComputer::SetIncremental(bool incremental) noexcept
{
  free.SetIncremental(incremental);
  dmst_quad.SetIncremental(incremental);
  dmst_triangle.SetIncremental(incremental);
  dmst_or.SetIncremental(incremental);
}

void
LogbookComputer::Reset() noexcept
{
  free.Reset();
  dmst_quad.Reset();
  dmst_triangle.Reset();
  dmst_or.Reset();

  result_free.Reset();
  result_quad.Reset();
  result_triangle.Reset();
  result_or.Reset();
}

/**
 * Run one solver; keep its previous result unless it found a new
 * one, so an incremental search does not lose the value shown.
 */
static void
Run(AbstractContest &solver, ContestResult &result, bool exhaustive) noexcept
{
  if (solver.Solve(exhaustive) == SolverResult::VALID)
    result = solver.GetBestResult();
}

void
LogbookComputer::Solve(unsigned handicap, bool exhaustive,
                       LogbookStatistics &stats) noexcept
{
  /* the solvers divide by the index; a plane without one counts as
     100, as the plane file does */
  if (handicap == 0)
    handicap = 100;

  free.SetHandicap(handicap);
  dmst_quad.SetHandicap(handicap);
  dmst_triangle.SetHandicap(handicap);
  dmst_or.SetHandicap(handicap);

  Run(free, result_free, exhaustive);
  Run(dmst_quad, result_quad, exhaustive);
  Run(dmst_triangle, result_triangle, exhaustive);
  Run(dmst_or, result_or, exhaustive);

  stats.free = result_free;

  /* each shape has its own bonus, which is part of its score; the
     best score decides, as in DMStFree */
  using Shape = LogbookStatistics::DMStShape;
  stats.dmst = result_quad;
  stats.dmst_shape = result_quad.IsDefined() ? Shape::QUADRILATERAL
                                             : Shape::NONE;

  if (result_triangle.IsDefined() &&
      result_triangle.score >= stats.dmst.score) {
    stats.dmst = result_triangle;
    stats.dmst_shape = Shape::TRIANGLE;
  }

  if (result_or.IsDefined() && result_or.score > stats.dmst.score) {
    stats.dmst = result_or;
    stats.dmst_shape = Shape::OUT_AND_RETURN;
  }
}
