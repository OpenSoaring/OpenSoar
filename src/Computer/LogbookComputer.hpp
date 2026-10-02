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
struct MoreData;
class Trace;

/**
 * Calculates the free distance and the DMSt result of the current
 * flight for the log book.  It runs beside the contest of the
 * settings (#ContestComputer), so the values are there whatever
 * contest the pilot has chosen, and it keeps them up to date in
 * flight.
 *
 * As in the contest rules, only the gliding flight counts: from the
 * release (or the end of the engine run of a self-launch) to the
 * landing.  An engine run in flight ends a part; the next part begins
 * when the engine is off again, and the best part counts.
 *
 * In flight the solvers work on the contest trace (256 points) of the
 * current part and search it completely once a minute.  On the full
 * trace (up to 25200 points) a search per fix costs too much, and a
 * search that is continued over several fixes is restarted by every
 * new fix and never finishes.  When a part ends, it is searched once
 * more on a copy of the full trace with 2048 points.
 */
class LogbookComputer {
  const Trace &trace_full, &trace_contest;

  /* the current part in flight, from the contest trace */
  Trace part_trace;
  OLCClassic free;
  DMStQuad dmst_quad;
  DMStTriangle dmst_triangle;
  DMStOR dmst_or;

  /**
   * A finished part, from the full trace.  On the contest trace the
   * triangle was up to 8% shorter than the one actually flown,
   * depending on which points the thinning kept (measured on flights
   * recorded by two loggers each).
   */
  Trace final_trace;
  OLCClassic final_free;
  DMStQuad final_quad;
  DMStTriangle final_triangle;
  DMStOR final_or;

  /** The results of the current part, from the last search in flight */
  ContestResult part_free, part_quad, part_triangle, part_or;

  /** The best results of the finished parts */
  ContestResult best_free, best_quad, best_triangle, best_or;

  /** Is a part running (gliding)? */
  bool in_part;

  /** The start of the current part */
  TimeStamp part_start;

  /** The start of the first part */
  TimeStamp release;

  unsigned finished_parts;

  /** The flight time of the last search in flight */
  FloatDuration last_solve;

  /*
   * The engine, from the noise sensor.  FlyingComputer's "powered" is
   * not used: its hysteresis (on above 500, off at 350) keeps it on
   * for minutes when a sensor hears 400 to 600 from the tow plane or
   * the airflow, which split flights without an engine run into
   * parts.  These are kept across Reset(), as the engine of a
   * self-launch is started on the ground.
   */

  /** Since when the noise has been above the threshold, without a pause */
  TimeStamp loud_since = TimeStamp::Undefined();

  /** The noise has been loud long enough for an engine run */
  bool engine_running = false;

  /** When the last engine run ended */
  TimeStamp engine_off = TimeStamp::Undefined();

  /**
   * Noise samples in flight, and those between quiet and an engine
   * (see IsEngineNoiseClear())
   */
  unsigned noise_samples, unclear_noise_samples;

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
  void Process(const MoreData &basic, const FlyingState &flight,
               unsigned handicap, bool exhaustive,
               LogbookStatistics &stats) noexcept;

  /**
   * The final calculation after the landing, or at the end of the
   * data (then the last fix ends the flight).
   */
  void SolveFinal(const FlyingState &flight, unsigned handicap,
                  LogbookStatistics &stats) noexcept;

private:
  void UpdateEngine(const MoreData &basic, bool flying) noexcept;
  bool IsEngineNoiseClear() const noexcept;
  void UpdateParts(const FlyingState &flight, unsigned handicap) noexcept;
  void BeginPart(TimeStamp start) noexcept;
  void EndPart(TimeStamp end, unsigned handicap) noexcept;

  /** Search the current part on the contest trace */
  void SolveInFlight(TimeStamp now, unsigned handicap) noexcept;

  void CopyResults(LogbookStatistics &stats) const noexcept;
};
