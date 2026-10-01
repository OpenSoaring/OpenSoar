// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Settings.hpp"

void
LoggerSettings::SetDefaults()
{
  time_step_cruise = std::chrono::seconds{5};
  time_step_circling = std::chrono::seconds{1};
  auto_logger = AutoLogger::ON;
  logger_id.clear();
  pilot_name.clear();
  copilot_name.clear();
  crew_mass_template = 90;

  /* the log book records the flights with all details now
     (logbook.csv); a pilot expects that without searching for it */
  enable_flight_logger = true;

  enable_nmea_logger = false;
}
