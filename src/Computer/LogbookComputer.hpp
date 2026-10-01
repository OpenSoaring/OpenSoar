// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Engine/Contest/Solvers/OLCClassic.hpp"
#include "Engine/Contest/Solvers/DMStQuad.hpp"
#include "Engine/Contest/Solvers/DMStTriangle.hpp"
#include "Engine/Contest/Solvers/DMStOR.hpp"
#include "Engine/Contest/ContestResult.hpp"

struct LogbookStatistics;
class Trace;

/**
 * Calculates the free distance and the DMSt result of the current
 * flight for the log book.  It runs beside the contest of the
 * settings (#ContestComputer), on the same traces, so the values are
 * there whatever contest the pilot has chosen, and it keeps them up
 * to date in flight, as the contest page does.
 */
class LogbookComputer {
  OLCClassic free;
  DMStQuad dmst_quad;
  DMStTriangle dmst_triangle;
  DMStOR dmst_or;

  ContestResult result_free;
  ContestResult result_quad, result_triangle, result_or;

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
};
