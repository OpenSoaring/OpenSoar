// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LogbookComputer.hpp"
#include "Engine/Contest/LogbookStatistics.hpp"
#include "NMEA/FlyingState.hpp"

/**
 * The size of the dense trace for the final triangle; the size of the
 * triangle trace of the contest analysis tool, whose results the
 * final values match.
 */
static constexpr unsigned FINAL_TRACE_SIZE = 1024;

/**
 * How often the contest trace is searched in flight.  A complete
 * search of its 256 points takes a few milliseconds on a PC.
 */
static constexpr FloatDuration IN_FLIGHT_INTERVAL = std::chrono::minutes{1};

LogbookComputer::LogbookComputer(const Trace &_trace_full,
                                 const Trace &trace_contest) noexcept
  :free(trace_contest),
   dmst_quad(trace_contest),
   /* the log book records what was flown, so the triangle is not
      assumed to be closed */
   dmst_triangle(trace_contest, false),
   dmst_or(trace_contest),
   final_free(_trace_full),
   final_quad(_trace_full),
   final_or(_trace_full),
   trace_full(_trace_full),
   final_trace({}, Trace::null_time, FINAL_TRACE_SIZE),
   final_triangle(final_trace, false)
{
  Reset();
}

void
LogbookComputer::Reset() noexcept
{
  free.Reset();
  dmst_quad.Reset();
  dmst_triangle.Reset();
  dmst_or.Reset();

  final_free.Reset();
  final_quad.Reset();
  final_or.Reset();
  final_trace.clear();
  final_triangle.Reset();

  result_free.Reset();
  result_quad.Reset();
  result_triangle.Reset();
  result_or.Reset();

  last_solve = FloatDuration{-1};
}

/**
 * Search completely with one solver.  Without a new result (the solver
 * reports none if the trace has not changed) the old one stays.
 *
 * @param replace true to take the new result even if it is not better
 */
static void
Run(AbstractContest &solver, unsigned handicap, ContestResult &result,
    bool replace) noexcept
{
  solver.SetHandicap(handicap);
  if (solver.Solve(true) != SolverResult::VALID)
    return;

  const ContestResult &found = solver.GetBestResult();
  if (replace || !result.IsDefined() || found.score > result.score)
    result = found;
}

/**
 * The solvers divide by the index; a plane without one counts as 100,
 * as the plane file does.
 */
static constexpr unsigned
ValidHandicap(unsigned handicap) noexcept
{
  return handicap > 0 ? handicap : 100;
}

void
LogbookComputer::SolveInFlight(unsigned handicap,
                               LogbookStatistics &stats) noexcept
{
  handicap = ValidHandicap(handicap);

  /* a new result replaces the old one even if the thinning of the
     trace makes it a little shorter, so the values follow the trace */
  Run(free, handicap, result_free, true);
  Run(dmst_quad, handicap, result_quad, true);
  Run(dmst_triangle, handicap, result_triangle, true);
  Run(dmst_or, handicap, result_or, true);

  CopyResults(stats);
}

void
LogbookComputer::CopyResults(LogbookStatistics &stats) const noexcept
{
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

void
LogbookComputer::Process(const FlyingState &flight, unsigned handicap,
                         bool exhaustive, LogbookStatistics &stats) noexcept
{
  /* after the landing, search once on the full trace: that result is
     final, and the log book waits for it */
  const bool landed = !flight.flying && flight.landing_time.IsDefined();

  if (landed && !stats.final) {
    SolveFinal(handicap, stats);
    stats.final = true;
  } else if (flight.flying) {
    stats.final = false;

    if (exhaustive || last_solve < FloatDuration{} ||
        flight.flight_time < last_solve ||
        flight.flight_time - last_solve >= IN_FLIGHT_INTERVAL) {
      last_solve = flight.flight_time;
      SolveInFlight(handicap, stats);
    }
  }
}

void
LogbookComputer::SolveFinal(unsigned handicap,
                            LogbookStatistics &stats) noexcept
{
  handicap = ValidHandicap(handicap);

  /* the last search in flight may be up to a minute old */
  SolveInFlight(handicap, stats);

  Run(final_free, handicap, result_free, false);
  Run(final_quad, handicap, result_quad, false);
  Run(final_or, handicap, result_or, false);

  final_trace.clear();
  for (const auto &point : trace_full)
    final_trace.push_back(point);

  /* only the triangle of the dense trace counts: the one found in
     flight on the contest trace was up to 14% longer than the triangle
     the contest analysis finds on the same flight (seen on 2025-05-12,
     619 instead of 545 km); the cause is not known yet */
  final_triangle.Reset();
  result_triangle.Reset();
  Run(final_triangle, handicap, result_triangle, true);

  CopyResults(stats);
}
