// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LogbookRecorder.hpp"
#include "NMEA/MoreData.hpp"
#include "NMEA/Derived.hpp"

#include <algorithm>
#include <cmath>

using std::chrono::seconds;
using std::chrono::minutes;

/**
 * A tow shorter than this was a winch launch; an aerotow takes
 * several minutes.
 */
static constexpr seconds MAX_WINCH_LAUNCH{120};

/**
 * An engine that runs this soon after the takeoff was the launch.
 */
static constexpr minutes MAX_SELF_LAUNCH_DELAY{3};

/**
 * Wait this long after the landing for the final distances before
 * writing the entry without them.
 */
static constexpr minutes MAX_FINAL_WAIT{2};

/**
 * The UTC date and time of an event of the calculation thread, from
 * the current GPS fix.
 */
[[gnu::pure]]
static BrokenDateTime
ToDateTime(const MoreData &basic, TimeStamp t) noexcept
{
  if (!t.IsDefined())
    return basic.date_time_utc;

  const auto ago = std::chrono::duration_cast<seconds>(basic.time - t);
  return basic.date_time_utc - ago;
}

void
LogbookRecorder::OnTakeoff(const MoreData &basic,
                           const DerivedInfo &calculated) noexcept
{
  const FlyingState &flight = calculated.flight;

  entry = {};
  entry.takeoff = ToDateTime(basic, flight.takeoff_time);
  entry.takeoff_place = handler.FindLogbookPlace(flight.takeoff_location);
  handler.OnLogbookTakeoff(entry);

  takeoff_time = flight.takeoff_time;
  engine_at_launch = false;
  in_flight = true;
  landed = false;
}

void
LogbookRecorder::OnFlying(const MoreData &basic,
                          const DerivedInfo &calculated) noexcept
{
  if (basic.NavAltitudeAvailable())
    entry.max_altitude = std::max(entry.max_altitude,
                                  (int)std::lround(basic.nav_altitude));

  /* the logger may start a little after the takeoff; remember the
     file it writes */
  if (entry.igc_file.empty())
    entry.igc_file = handler.GetLogbookIgcFile();

  const FlyingState &flight = calculated.flight;
  if (flight.power_on_time.IsDefined() && takeoff_time.IsDefined() &&
      flight.power_on_time - takeoff_time < MAX_SELF_LAUNCH_DELAY)
    engine_at_launch = true;
}

void
LogbookRecorder::SetLaunch(const FlyingState &flight) noexcept
{
  if (engine_at_launch)
    entry.launch = LogbookEntry::Launch::SELF;
  else if (flight.release_time.IsDefined() && takeoff_time.IsDefined())
    entry.launch = flight.release_time - takeoff_time <= MAX_WINCH_LAUNCH
      ? LogbookEntry::Launch::WINCH
      : LogbookEntry::Launch::AEROTOW;
}

void
LogbookRecorder::OnLanding(const MoreData &basic,
                           const DerivedInfo &calculated) noexcept
{
  const FlyingState &flight = calculated.flight;

  entry.landing = ToDateTime(basic, flight.landing_time);
  entry.landing_place = handler.FindLogbookPlace(flight.landing_location);
  SetLaunch(flight);

  landed = true;
  landed_at = basic.time;
}

void
LogbookRecorder::Finish(const DerivedInfo &calculated) noexcept
{
  const LogbookStatistics &stats = calculated.logbook_stats;
  entry.free_distance = stats.free.distance;
  entry.dmst_distance = stats.dmst.distance;
  entry.dmst_points = stats.dmst.score;
  entry.dmst_shape = stats.dmst_shape;

  in_flight = false;
  landed = false;

  handler.OnLogbookFlight(entry);
}

void
LogbookRecorder::Update(const MoreData &basic,
                        const DerivedInfo &calculated) noexcept
{
  if (!basic.time_available || !basic.date_time_utc.IsDatePlausible())
    return;

  const FlyingState &flight = calculated.flight;

  if (!in_flight) {
    /* a flight begins once it is confirmed (in the air, not just
       rolling) */
    if (flight.flying && !flight.on_ground)
      OnTakeoff(basic, calculated);
    else
      return;
  }

  if (!landed) {
    if (flight.flying) {
      OnFlying(basic, calculated);
      return;
    }

    if (!flight.landing_time.IsDefined())
      return;

    OnLanding(basic, calculated);
  }

  /* landed: the calculation thread searches the distances once more
     after the landing; wait for that, but not for ever */
  if (calculated.logbook_stats.final ||
      basic.time - landed_at >= MAX_FINAL_WAIT ||
      basic.time < landed_at)
    Finish(calculated);
}

void
LogbookRecorder::FinishAtEnd(const MoreData &basic,
                             const DerivedInfo &calculated,
                             const char *remark) noexcept
{
  if (!in_flight)
    return;

  if (!landed) {
    entry.landing = basic.date_time_utc;
    entry.landing_place = handler.FindLogbookPlace(basic.location);
    SetLaunch(calculated.flight);
  }

  entry.remark = remark;
  Finish(calculated);
}
