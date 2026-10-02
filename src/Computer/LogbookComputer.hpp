// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Engine/Contest/Solvers/OLCClassic.hpp"
#include "Engine/Contest/Solvers/DMStQuad.hpp"
#include "Engine/Contest/Solvers/DMStTriangle.hpp"
#include "Engine/Contest/Solvers/DMStOR.hpp"
#include "Engine/Contest/ContestResult.hpp"
#include "Engine/Trace/Trace.hpp"
#include "time/FloatDuration.hxx"

struct LogbookStatistics;
struct FlyingState;
class Trace;

/**
 * Calculates the free distance and the DMSt result of the current
 * flight for the log book.  It runs beside the contest of the
 * settings (#ContestComputer), so the values are there whatever
 * contest the pilot has chosen, and it keeps them up to date in
 * flight.
 *
 * In flight the solvers work on the contest trace (256 points) and
 * search it completely once a minute.  On the full trace (up to 25200
 * points) a search per fix costs too much, and a search that is
 * continued over several fixes is restarted by every new fix and
 * never finishes: the values stayed at the first few hundred metres
 * for the whole flight (seen in RunLogbook, which calls this once per
 * fix as the calculation thread does).  After the landing everything
 * is searched once on the full trace, which gives the values the
 * contest analysis gives.
 */
class LogbookComputer {
  /* in flight, on the contest trace */
  OLCClassic free;
  DMStQuad dmst_quad;
  DMStTriangle dmst_triangle;
  DMStOR dmst_or;

  /* after the landing, on the full trace */
  OLCClassic final_free;
  DMStQuad final_quad;
  DMStOR final_or;

  /**
   * The triangle after the landing, on a copy of the full trace with
   * 1024 points: on the contest trace it was up to 8% shorter than the
   * one actually flown, depending on which points the thinning kept,
   * and the full trace is too large for the triangle search (measured
   * on flights recorded by two loggers each).
   */
  const Trace &trace_full;
  Trace final_trace;
  DMStTriangle final_triangle;

  ContestResult result_free;
  ContestResult result_quad, result_triangle, result_or;

  /** The flight time of the last search in flight */
  FloatDuration last_solve;

public:
  LogbookComputer(const Trace &trace_full,
                  const Trace &trace_contest) noexcept;

  void Reset() noexcept;

  /**
   * The calculation for one idle pass: in flight once a minute (or
   * when @p exhaustive is set), once after the landing (then
   * #LogbookStatistics::final is set), nothing on the ground before
   * or after.
   */
  void Process(const FlyingState &flight, unsigned handicap,
               bool exhaustive, LogbookStatistics &stats) noexcept;

  /**
   * The final calculation after the landing, on the full trace.
   */
  void SolveFinal(unsigned handicap, LogbookStatistics &stats) noexcept;

private:
  /** Search the contest trace completely */
  void SolveInFlight(unsigned handicap, LogbookStatistics &stats) noexcept;

  void CopyResults(LogbookStatistics &stats) const noexcept;
};
