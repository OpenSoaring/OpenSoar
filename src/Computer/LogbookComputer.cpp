// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LogbookComputer.hpp"
#include "Engine/Contest/LogbookStatistics.hpp"
#include "NMEA/FlyingState.hpp"
#include "NMEA/MoreData.hpp"

/**
 * The size of the traces of a part: in flight the contest trace has
 * 256 points.  For a finished part, 2048 points: with 1024 (the size
 * the contest analysis tool uses for the triangle) the triangle of
 * 2022-04-29 was 463 km in one logger's file and 488 km in another's,
 * with 2048 it is 502 km in both, and a part takes about two seconds
 * on a PC.
 */
static constexpr unsigned PART_TRACE_SIZE = 256;
static constexpr unsigned FINAL_TRACE_SIZE = 2048;

/**
 * How often the current part is searched in flight.  A complete
 * search of its 256 points takes a few milliseconds on a PC.
 */
static constexpr FloatDuration IN_FLIGHT_INTERVAL = std::chrono::minutes{1};

/**
 * Engine noise above this (of 999) for at least #MIN_ENGINE_RUN
 * without a pause is an engine run.  A running engine gives 990 and
 * more in the self-launches of the pilot's collection that were
 * checked.  FlyingComputer's threshold of 500 is too low here: some
 * FLARM loggers report 600 to 900 from the airflow for the whole
 * flight, with runs above 500 of up to four minutes, and a flight was
 * cut into 61 parts.
 */
static constexpr unsigned ENGINE_NOISE = 900;
static constexpr FloatDuration MIN_ENGINE_RUN = std::chrono::minutes{1};

/**
 * A part that an engine run ends sooner than this was no part: the
 * engine of a self-launch reached the threshold only after the
 * release detection of FlyingComputer had seen a pause in the climb
 * (2025-05-24: "release" 13 seconds after the takeoff).
 */
static constexpr FloatDuration MIN_PART = std::chrono::minutes{3};

LogbookComputer::LogbookComputer(const Trace &_trace_full,
                                 const Trace &_trace_contest) noexcept
  :trace_full(_trace_full), trace_contest(_trace_contest),
   part_trace({}, Trace::null_time, PART_TRACE_SIZE),
   free(part_trace),
   dmst_quad(part_trace),
   /* the log book records what was flown, so the triangle is not
      assumed to be closed */
   dmst_triangle(part_trace, false),
   dmst_or(part_trace),
   final_trace({}, Trace::null_time, FINAL_TRACE_SIZE),
   final_free(final_trace),
   final_quad(final_trace),
   final_triangle(final_trace, false),
   final_or(final_trace)
{
  Reset();
}

void
LogbookComputer::Reset() noexcept
{
  part_trace.clear();
  free.Reset();
  dmst_quad.Reset();
  dmst_triangle.Reset();
  dmst_or.Reset();

  final_trace.clear();
  final_free.Reset();
  final_quad.Reset();
  final_triangle.Reset();
  final_or.Reset();

  part_free.Reset();
  part_quad.Reset();
  part_triangle.Reset();
  part_or.Reset();

  best_free.Reset();
  best_quad.Reset();
  best_triangle.Reset();
  best_or.Reset();

  in_part = false;
  part_start = TimeStamp::Undefined();
  release = TimeStamp::Undefined();
  finished_parts = 0;
  last_solve = FloatDuration{-1};
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

[[gnu::pure]]
static bool
IsBetter(const ContestResult &a, const ContestResult &b) noexcept
{
  return a.IsDefined() && (!b.IsDefined() || a.score > b.score);
}

/**
 * Search completely with one solver.  Without a new result (the solver
 * reports none if the trace has not changed) the old one stays.
 */
static void
Run(AbstractContest &solver, unsigned handicap,
    ContestResult &result) noexcept
{
  solver.SetHandicap(handicap);
  if (solver.Solve(true) == SolverResult::VALID)
    result = solver.GetBestResult();
}

/**
 * Copy the points of one part of the flight.
 */
static void
CopyPart(const Trace &src, Trace &dest, TimeStamp start,
         TimeStamp end) noexcept
{
  const auto begin_time = start.Cast<TracePoint::Time>();
  const auto end_time = end.Cast<TracePoint::Time>();

  dest.clear();
  for (const auto &point : src)
    if (point.GetTime() >= begin_time && point.GetTime() <= end_time)
      dest.push_back(point);
}

void
LogbookComputer::BeginPart(TimeStamp start) noexcept
{
  in_part = true;
  part_start = start;
  if (!release.IsDefined())
    release = start;

  part_free.Reset();
  part_quad.Reset();
  part_triangle.Reset();
  part_or.Reset();
  last_solve = FloatDuration{-1};
}

void
LogbookComputer::SolveInFlight(TimeStamp now, unsigned handicap) noexcept
{
  handicap = ValidHandicap(handicap);

  CopyPart(trace_contest, part_trace, part_start, now);

  /* a new result replaces the old one even if the thinning of the
     trace makes it a little shorter, so the values follow the trace */
  Run(free, handicap, part_free);
  Run(dmst_quad, handicap, part_quad);
  Run(dmst_triangle, handicap, part_triangle);
  Run(dmst_or, handicap, part_or);
}

void
LogbookComputer::EndPart(TimeStamp end, unsigned handicap) noexcept
{
  handicap = ValidHandicap(handicap);

  /* the last search in flight may be up to a minute old */
  SolveInFlight(end, handicap);

  CopyPart(trace_full, final_trace, part_start, end);

  ContestResult result_free, result_quad, result_triangle, result_or;
  result_free.Reset();
  result_quad.Reset();
  result_triangle.Reset();
  result_or.Reset();

  final_free.Reset();
  final_quad.Reset();
  final_triangle.Reset();
  final_or.Reset();
  Run(final_free, handicap, result_free);
  Run(final_quad, handicap, result_quad);
  Run(final_triangle, handicap, result_triangle);
  Run(final_or, handicap, result_or);

  /* each search may miss the best route, depending on which points
     the thinning of its trace kept, so the better one counts.  On
     2022-05-29 the full trace gave an out and return of 417 points
     instead of the quadrilateral of 420 points the contest analysis
     finds.  The triangle search also stops when its tree grows too
     large: on 2025-05-12 the contest analysis finds 619 km in the file
     of one logger and 544 km in the file of another one. */
  if (IsBetter(part_free, result_free))
    result_free = part_free;
  if (IsBetter(part_quad, result_quad))
    result_quad = part_quad;
  if (IsBetter(part_triangle, result_triangle))
    result_triangle = part_triangle;
  if (IsBetter(part_or, result_or))
    result_or = part_or;

  if (IsBetter(result_free, best_free))
    best_free = result_free;
  if (IsBetter(result_quad, best_quad))
    best_quad = result_quad;
  if (IsBetter(result_triangle, best_triangle))
    best_triangle = result_triangle;
  if (IsBetter(result_or, best_or))
    best_or = result_or;

  in_part = false;
  ++finished_parts;
}

/**
 * The current time of the calculation thread, from the flight state.
 */
[[gnu::pure]]
static TimeStamp
Now(const FlyingState &flight) noexcept
{
  return flight.takeoff_time + flight.flight_time;
}

void
LogbookComputer::UpdateEngine(const MoreData &basic) noexcept
{
  if (!basic.engine_noise_level_available || !basic.time_available)
    return;

  if (basic.engine_noise_level > ENGINE_NOISE) {
    if (!loud_since.IsDefined() || basic.time < loud_since)
      loud_since = basic.time;
    if (!engine_running && basic.time - loud_since >= MIN_ENGINE_RUN)
      engine_running = true;
  } else {
    loud_since = TimeStamp::Undefined();
    if (engine_running) {
      engine_running = false;
      engine_off = basic.time;
    }
  }
}

void
LogbookComputer::UpdateParts(const FlyingState &flight,
                             unsigned handicap) noexcept
{
  /* gliding: flying, released from the tow or winch and the engine
     off.  A part does not begin while the engine is loud, even before
     that counts as an engine run: FlyingComputer may see a "release"
     in the first seconds of a self-launch, when the climb pauses. */
  const bool gliding = flight.flying && flight.release_time.IsDefined() &&
    !engine_running && !loud_since.IsDefined();

  if (!in_part && gliding) {
    /* the part begins at the release, or for a self-launch (and
       after an engine run in flight) when the engine was switched
       off */
    TimeStamp start = flight.release_time;
    if (engine_off.IsDefined() && engine_off > start &&
        engine_off >= flight.takeoff_time)
      start = engine_off;
    BeginPart(start);
  } else if (in_part && engine_running) {
    /* an engine run ends the part, from its beginning */
    TimeStamp end = loud_since;
    if (!end.IsDefined() || end < part_start)
      end = Now(flight);

    if (end - part_start < MIN_PART) {
      in_part = false;
      if (finished_parts == 0)
        release = TimeStamp::Undefined();
    } else
      EndPart(end, handicap);
  }
}

void
LogbookComputer::CopyResults(LogbookStatistics &stats) const noexcept
{
  ContestResult result_free = best_free, result_quad = best_quad,
    result_triangle = best_triangle, result_or = best_or;

  if (in_part) {
    if (IsBetter(part_free, result_free))
      result_free = part_free;
    if (IsBetter(part_quad, result_quad))
      result_quad = part_quad;
    if (IsBetter(part_triangle, result_triangle))
      result_triangle = part_triangle;
    if (IsBetter(part_or, result_or))
      result_or = part_or;
  }

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

  stats.release = release;
  stats.scored_parts = finished_parts + (in_part ? 1 : 0);
}

void
LogbookComputer::Process(const MoreData &basic, const FlyingState &flight,
                         unsigned handicap, bool exhaustive,
                         LogbookStatistics &stats) noexcept
{
  UpdateEngine(basic);

  /* after the landing, search once on the full trace: that result is
     final, and the log book waits for it */
  const bool landed = !flight.flying && flight.landing_time.IsDefined();

  if (landed && !stats.final) {
    SolveFinal(flight, handicap, stats);
    stats.final = true;
  } else if (flight.flying) {
    stats.final = false;

    UpdateParts(flight, handicap);

    if (in_part &&
        (exhaustive || last_solve < FloatDuration{} ||
         flight.flight_time < last_solve ||
         flight.flight_time - last_solve >= IN_FLIGHT_INTERVAL)) {
      last_solve = flight.flight_time;
      SolveInFlight(Now(flight), handicap);
    }

    CopyResults(stats);
  }
}

void
LogbookComputer::SolveFinal(const FlyingState &flight, unsigned handicap,
                            LogbookStatistics &stats) noexcept
{
  const TimeStamp end = flight.landing_time.IsDefined() && !flight.flying
    ? flight.landing_time
    : Now(flight);

  if (in_part)
    EndPart(end, handicap);

  if (finished_parts == 0 && flight.takeoff_time.IsDefined()) {
    /* no release was seen (e.g. no altitude, or a flight shorter than
       the release detection needs): the whole flight counts */
    BeginPart(flight.takeoff_time);
    release = TimeStamp::Undefined();
    EndPart(end, handicap);
  }

  CopyResults(stats);
}
