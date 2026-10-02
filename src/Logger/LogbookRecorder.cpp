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
 * A winch launch climbs this much within #WINCH_CLIMB_TIME of the
 * takeoff; an aerotow or a self-launch climbs 2 to 4 m/s and reaches
 * about half of it.  Measured on 15 flights recorded by two or three
 * loggers each: winch launches 317 to 450 m, aerotows 93 to 205 m.
 */
static constexpr double WINCH_CLIMB = 250;
static constexpr seconds WINCH_CLIMB_TIME{60};

/**
 * An engine that runs this soon after the takeoff was the launch.
 */
static constexpr minutes MAX_SELF_LAUNCH_DELAY{3};

/**
 * A ground speed above this (m/s) at the end of the data means the
 * aircraft was still in the air.
 */
static constexpr double MIN_AIR_SPEED = 15;

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

  began_in_air = !seen_ground;
  if (began_in_air)
    /* the first fixes in the air are no takeoff place, and the time
       is when the recording began */
    entry.remark = "recording began in flight";
  else
    entry.takeoff_place = handler.FindLogbookPlace(flight.takeoff_location);

  handler.OnLogbookTakeoff(entry);
  seen_ground = false;

  takeoff_time = flight.takeoff_time;
  takeoff_altitude = flight.takeoff_altitude;
  engine_at_launch = false;
  launch_checked = false;
  winch_climb = false;
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
  if (entry.log_file.empty()) {
    entry.log_file = handler.GetLogbookFile();
    entry.file_type = Logbook::FileTypeOf(entry.log_file);
  }

  const FlyingState &flight = calculated.flight;
  if (flight.power_on_time.IsDefined() && takeoff_time.IsDefined() &&
      flight.power_on_time - takeoff_time < MAX_SELF_LAUNCH_DELAY)
    engine_at_launch = true;

  if (!launch_checked && takeoff_time.IsDefined() &&
      basic.NavAltitudeAvailable() &&
      basic.time - takeoff_time >= WINCH_CLIMB_TIME) {
    winch_climb = basic.nav_altitude - takeoff_altitude >= WINCH_CLIMB;
    launch_checked = true;
  }
}

void
LogbookRecorder::SetLaunch(const FlyingState &flight) noexcept
{
  if (began_in_air)
    /* the climb, the engine and the release of the real launch
       were not seen */
    return;

  /* the climb tells a winch launch; the noise a winch launch makes
     in an engine noise sensor must not make it a self-launch, so it
     is checked first */
  if (winch_climb)
    entry.launch = LogbookEntry::Launch::WINCH;
  else if (engine_at_launch)
    entry.launch = LogbookEntry::Launch::SELF;
  else if (flight.release_time.IsDefined())
    entry.launch = LogbookEntry::Launch::AEROTOW;
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
  /* the time of the free distance runs from its first to its last
     point, not from takeoff to landing, so a long local soaring at
     the end does not lower it */
  entry.free_speed = stats.free.GetSpeed() * 3.6;
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
  const FlyingState &flight = calculated.flight;

  if (flight.on_ground)
    seen_ground = true;

  if (!basic.time_available || !basic.date_time_utc.IsDatePlausible())
    return;

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

    /* faster than any landing roll: the recording stopped in the air
       (an app or logger restart), and the last fix is no landing
       place */
    const bool in_air = basic.ground_speed_available &&
      basic.ground_speed > MIN_AIR_SPEED;
    if (in_air)
      remark = "recording ended in flight";
    else
      entry.landing_place = handler.FindLogbookPlace(basic.location);

    SetLaunch(calculated.flight);
  }

  if (entry.remark.empty())
    entry.remark = remark;
  else
    entry.remark = entry.remark + ", " + remark;
  Finish(calculated);
}
