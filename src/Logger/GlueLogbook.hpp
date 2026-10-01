// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Blackboard/BlackboardListener.hpp"
#include "Logbook.hpp"
#include "system/Path.hpp"
#include "time/Stamp.hpp"

class LiveBlackboard;
class Waypoints;
class Logger;
struct GeoPoint;

/**
 * Records each flight in the log book (logbook.csv): takeoff and
 * landing with time and place, launch, crew, aircraft, the free and
 * DMSt distance, the maximum altitude and the IGC file.
 *
 * It follows the flight state of the live blackboard (main thread)
 * and writes the entry once the landing is confirmed and the
 * calculation thread has the final distances (#LogbookStatistics).
 */
class GlueLogbook final : private NullBlackboardListener {
  LiveBlackboard &blackboard;
  const Waypoints *const waypoints;
  const Logger *const igc_logger;

  const AllocatedPath path;

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

public:
  GlueLogbook(LiveBlackboard &blackboard, Path path,
              const Waypoints *waypoints,
              const Logger *igc_logger) noexcept;

  ~GlueLogbook() noexcept;

  GlueLogbook(const GlueLogbook &) = delete;
  GlueLogbook &operator=(const GlueLogbook &) = delete;

private:
  void OnTakeoff(const MoreData &basic, const DerivedInfo &calculated);
  void OnFlying(const MoreData &basic, const DerivedInfo &calculated);
  void OnLanding(const MoreData &basic, const DerivedInfo &calculated);
  void Finish(const DerivedInfo &calculated);

  [[gnu::pure]]
  std::string FindPlace(const GeoPoint &location) const noexcept;

  /* virtual methods from class BlackboardListener */
  void OnCalculatedUpdate(const MoreData &basic,
                          const DerivedInfo &calculated) override;
};
