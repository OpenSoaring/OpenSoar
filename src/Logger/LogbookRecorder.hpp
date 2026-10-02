// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Logbook.hpp"
#include "time/Stamp.hpp"

#include <optional>
#include <string>

struct MoreData;
struct DerivedInfo;
struct GeoPoint;
struct FlyingState;

/**
 * Follows the flight state and assembles one #LogbookEntry per
 * flight: times and places of takeoff and landing, the launch, the
 * maximum altitude and the final distances.
 *
 * It knows nothing about threads, files or settings; the program
 * (#GlueLogbook) and the test tool RunLogbook feed it the same data
 * and decide where the entry goes, so a test of the tool with real
 * flights tests what the program does.
 */
class LogbookRecorder {
public:
  class Handler {
  public:
    /**
     * The takeoff was confirmed: fill in what the computer cannot
     * see in the flight (crew, aircraft).
     */
    virtual void OnLogbookTakeoff(LogbookEntry &entry) noexcept = 0;

    /**
     * A name for this place (airfield, waypoint or coordinates).
     */
    virtual std::string FindLogbookPlace(const GeoPoint &location) noexcept = 0;

    /**
     * The elevation of an airfield near this place, if there is one
     * (see Logbook::FindAirfieldElevation()).
     */
    virtual std::optional<double>
    GetLogbookAirfieldElevation([[maybe_unused]] const GeoPoint &location) noexcept {
      return std::nullopt;
    }

    /**
     * The name of the flight log file being written (IGC, or NMEA in
     * the test tool), or an empty string.
     */
    virtual std::string GetLogbookFile() noexcept {
      return {};
    }

    /**
     * The log file was found: fill in the flight recorder that
     * writes it (#LogbookEntry::recorder_code and recorder_type).
     */
    virtual void FillLogbookRecorder([[maybe_unused]] LogbookEntry &entry) noexcept {}

    /**
     * The flight is complete; this is the place to store it.
     */
    virtual void OnLogbookFlight(const LogbookEntry &entry) noexcept = 0;
  };

private:
  Handler &handler;

  /** The flight being recorded; valid while #in_flight */
  LogbookEntry entry;

  bool in_flight = false;

  /** The landing was confirmed, waiting for the final distances */
  bool landed = false;

  /** When the landing was confirmed (to stop waiting at some point) */
  TimeStamp landed_at;

  /** When the flight began (calculation thread's clock) */
  TimeStamp takeoff_time;

  /** The engine was running at the launch: a self-launch */
  bool engine_at_launch = false;

  /**
   * Since when the engine noise has been above the threshold without
   * a pause; undefined while it is quiet or there is no sensor
   */
  TimeStamp loud_since = TimeStamp::Undefined();

  /** The altitude at the takeoff, to tell a winch launch by its climb */
  double takeoff_altitude;

  /** The climb in the first minute has been checked */
  bool launch_checked = false;

  /** That climb was a winch launch */
  bool winch_climb = false;

  /**
   * The aircraft was seen standing on the ground since the last
   * flight (or since the start).  A takeoff without it means the
   * recording began in the air, after a restart of the program or
   * of the logger: the takeoff is not the real one and the launch
   * cannot be told.
   */
  bool seen_ground = false;

  /** This flight was first seen in the air */
  bool began_in_air = false;

public:
  explicit LogbookRecorder(Handler &_handler) noexcept
    :handler(_handler) {}

  /**
   * Call with each new calculation result.
   */
  void Update(const MoreData &basic, const DerivedInfo &calculated) noexcept;

  /**
   * The data end while a flight is being recorded (the end of a file,
   * or the program stops in the air): complete the entry at the last
   * fix and hand it to the handler.  If the aircraft was still moving
   * fast, the recording ended in the air: the landing place stays
   * empty and the remark says so; otherwise it had just landed, and
   * the given remark is added.
   *
   * @param calculated the final distances should be in
   * #DerivedInfo::logbook_stats (LogbookComputer::SolveFinal())
   */
  void FinishAtEnd(const MoreData &basic, const DerivedInfo &calculated,
                   const char *remark) noexcept;

  /**
   * Did the flight being recorded begin in the air (see
   * #seen_ground)?
   */
  bool BeganInAir() const noexcept {
    return began_in_air;
  }

  /**
   * Is a flight being recorded (taken off, not yet written)?
   */
  bool IsInFlight() const noexcept {
    return in_flight;
  }

  /**
   * The flight being recorded so far.
   */
  const LogbookEntry &GetEntry() const noexcept {
    return entry;
  }

private:
  void OnTakeoff(const MoreData &basic, const DerivedInfo &calculated) noexcept;
  void OnFlying(const MoreData &basic, const DerivedInfo &calculated) noexcept;
  void OnLanding(const MoreData &basic, const DerivedInfo &calculated) noexcept;
  void SetLaunch(const FlyingState &flight) noexcept;
  void AddRemark(std::string_view remark) noexcept;
  bool IsGroundStart(const MoreData &basic,
                     const DerivedInfo &calculated) noexcept;
  void Finish(const DerivedInfo &calculated) noexcept;
};
