// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Engine/Contest/Solvers/OLCClassic.hpp"
#include "Engine/Contest/Solvers/DMStQuad.hpp"
#include "Engine/Contest/Solvers/DMStTriangle.hpp"
#include "Engine/Contest/Solvers/DMStOR.hpp"
#include "Engine/Contest/ContestResult.hpp"
#include "Engine/Trace/Trace.hpp"

struct LogbookStatistics;
struct FlyingState;
class Trace;

/**
 * Calculates the free distance and the DMSt result of the current
 * flight for the log book.  It runs beside the contest of the
 * settings (#ContestComputer), on the same traces, so the values are
 * there whatever contest the pilot has chosen, and it keeps them up
 * to date in flight, as the contest page does.
 */
class LogbookComputer {
  const Trace &trace_full;

  OLCClassic free;
  DMStQuad dmst_quad;
  DMStTriangle dmst_triangle;
  DMStOR dmst_or;

  ContestResult result_free;
  ContestResult result_quad, result_triangle, result_or;

  /**
   * For the final result after the landing: the triangle searched
   * once more on a denser copy of the full trace.  The contest trace
   * of the flight keeps only 256 points to save time in flight; over
   * a long flight that is one point every one or two minutes, and the
   * triangle found on it was up to 8% shorter than the one actually
   * flown, depending on which points the thinning kept (measured on
   * flights recorded by two loggers each).
   */
  Trace final_trace;
  DMStTriangle final_triangle;

public:
  LogbookComputer(const Trace &trace_full,
                  const Trace &trace_triangle) noexcept;

  void SetIncremental(bool incremental) noexcept;

  void Reset() noexcept;

  /**
   * Continue the calculation and copy the results.
   *
   * @param handicap the index of the plane (100 if unknown)
   * @param exhaustive true to find the final solution, false to stop
   * after a number of iterations and continue in the next call
   */
  void Solve(unsigned handicap, bool exhaustive,
             LogbookStatistics &stats) noexcept;

  /**
   * The calculation for one idle pass: incremental in flight, once
   * exhaustive after the landing (then #LogbookStatistics::final is
   * set), nothing on the ground before or after.
   */
  void Process(const FlyingState &flight, unsigned handicap,
               bool exhaustive, LogbookStatistics &stats) noexcept;

  /**
   * The final calculation after the landing: everything exhaustively,
   * the triangle on the dense copy of the full trace.
   */
  void SolveFinal(unsigned handicap, LogbookStatistics &stats) noexcept;

private:
  void CopyResults(LogbookStatistics &stats) const noexcept;
};
