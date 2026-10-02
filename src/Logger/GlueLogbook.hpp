// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Blackboard/BlackboardListener.hpp"
#include "LogbookRecorder.hpp"
#include "system/Path.hpp"

class LiveBlackboard;
class Waypoints;
class Logger;
struct GeoPoint;

/**
 * Records each flight in the log book (logbook.csv).
 *
 * The flight is assembled by #LogbookRecorder from the live
 * blackboard (main thread); this class adds what the program knows
 * beyond the flight (crew and plane from the settings, place names
 * from the waypoints, the IGC file of the logger) and appends the
 * finished entry to the file.
 */
class GlueLogbook final
  : private NullBlackboardListener, LogbookRecorder::Handler {
  LiveBlackboard &blackboard;
  const Waypoints *const waypoints;
  const Logger *const igc_logger;

  const AllocatedPath path;

  LogbookRecorder recorder{*this};

public:
  GlueLogbook(LiveBlackboard &blackboard, Path path,
              const Waypoints *waypoints,
              const Logger *igc_logger) noexcept;

  ~GlueLogbook() noexcept;

  GlueLogbook(const GlueLogbook &) = delete;
  GlueLogbook &operator=(const GlueLogbook &) = delete;

private:
  /* virtual methods from class LogbookRecorder::Handler */
  void OnLogbookTakeoff(LogbookEntry &entry) noexcept override;
  std::string FindLogbookPlace(const GeoPoint &location) noexcept override;
  std::string GetLogbookFile() noexcept override;
  void OnLogbookFlight(const LogbookEntry &entry) noexcept override;

  /* virtual methods from class BlackboardListener */
  void OnCalculatedUpdate(const MoreData &basic,
                          const DerivedInfo &calculated) override;
};
