// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "util/StaticString.hxx"

#include <chrono>

/**
 * Logger settings
 */
struct LoggerSettings {
  /**
   * Enable the log book (#GlueLogbook, and #FlightLogger for the plain
   * list flights.log)?
   */
  bool enable_flight_logger;

  /**
   * Enable the #NMEALogger?
   */
  bool enable_nmea_logger;

  /** Logger interval in cruise mode */
  std::chrono::duration<unsigned> time_step_cruise;

  /** Logger interval in circling mode */
  std::chrono::duration<unsigned> time_step_circling;

  enum class AutoLogger: uint8_t {
    ON,
    START_ONLY,
    OFF,
  } auto_logger;

  StaticString<32> logger_id;

  StaticString<64> pilot_name;

  StaticString<64> copilot_name;

  /** Crew mass template in kg */
  unsigned crew_mass_template;

  /**
   * The external flight recorders of this glider, Recorder 1 and 2,
   * written as in the A record of their IGC files: the three letter
   * manufacturer code followed by the serial number ("LXVJNI"); empty
   * if not set.  Their files give the log book entry of a flight
   * before the program's own IGC file.
   */
  StaticString<16> recorders[2];

  /**
   * The device of Recorder 1 and 2: 1 for device A, 2 for B and so
   * on, 0 for none.  After the landing, the newest flight is read from
   * it (GlueRecorderDownload); its driver is the type of the recorder.
   */
  uint8_t recorder_devices[2];

  void SetDefaults();
};
