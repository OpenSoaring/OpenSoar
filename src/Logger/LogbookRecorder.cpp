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
 * An engine that runs this soon after the takeoff, and then for
 * #MIN_SELF_LAUNCH_RUN without a pause, was the launch.  The engine
 * noise sensor also hears the tow plane: in 73 aerotows of IGC files
 * of 2018 to 2026 it measured more than 500 of 999 for up to 133
 * seconds without a pause, while the engines of 59 self-launches ran
 * for 252 seconds and more.  Two of those aerotows, confirmed by the
 * pilot, had been taken for self-launches by the previous rule (any
 * engine within three minutes).
 */
static constexpr minutes MAX_SELF_LAUNCH_DELAY{3};
static constexpr minutes MIN_SELF_LAUNCH_RUN{3};

/**
 * Engine noise above this (of 999) counts as a running engine, the
 * threshold FlyingComputer uses as well.
 */
static constexpr unsigned ENGINE_NOISE = 500;

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

/**
 * The takeoff was this close above the ground: it was one.
 */
static constexpr double MAX_GROUND_START_HEIGHT = 150;

bool
LogbookRecorder::IsGroundStart(const MoreData &basic,
                               const DerivedInfo &calculated) noexcept
{
  /* many IGC loggers begin to record only when the aircraft moves, a
     few seconds before or at the takeoff, so the aircraft is not seen
     standing on the ground; but it is low near an airfield, which a
     restart in the air hardly ever is */
  const FlyingState &flight = calculated.flight;

  if (calculated.altitude_agl_valid)
    return calculated.altitude_agl < MAX_GROUND_START_HEIGHT;

  const auto elevation =
    handler.GetLogbookAirfieldElevation(flight.takeoff_location);
  if (!elevation)
    return false;

  if (!basic.GetAnyAltitude())
    /* FlyingComputer had no altitude for the takeoff either */
    return false;

  return flight.takeoff_altitude - *elevation < MAX_GROUND_START_HEIGHT;
}

void
LogbookRecorder::OnTakeoff(const MoreData &basic,
                           const DerivedInfo &calculated) noexcept
{
  const FlyingState &flight = calculated.flight;

  entry = {};
  entry.takeoff = ToDateTime(basic, flight.takeoff_time);

  began_in_air = !seen_ground && !IsGroundStart(basic, calculated);
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
                          [[maybe_unused]] const DerivedInfo &calculated) noexcept
{
  if (basic.NavAltitudeAvailable())
    entry.max_altitude = std::max(entry.max_altitude,
                                  (int)std::lround(basic.nav_altitude));

  /* the logger may start a little after the takeoff; remember the
     file it writes */
  if (entry.log_file.empty()) {
    entry.log_file = handler.GetLogbookFile();
    entry.file_type = Logbook::FileTypeOf(entry.log_file);
    if (!entry.log_file.empty())
      handler.FillLogbookRecorder(entry);
  }

  if (loud_since.IsDefined() && takeoff_time.IsDefined() &&
      loud_since - takeoff_time < MAX_SELF_LAUNCH_DELAY &&
      basic.time - loud_since >= MIN_SELF_LAUNCH_RUN)
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

  /* the engine of a self-launch runs from before the takeoff, so
     this is followed on the ground as well.  FlyingComputer's
     "powered" is not used: its hysteresis (on above 500, off at 350)
     keeps it on through the 400 to 600 a tow plane makes in some
     sensors for minutes. */
  if (basic.engine_noise_level_available) {
    if (basic.engine_noise_level <= ENGINE_NOISE)
      loud_since = TimeStamp::Undefined();
    else if (!loud_since.IsDefined())
      loud_since = basic.time;
  }

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
