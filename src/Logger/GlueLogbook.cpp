// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "GlueLogbook.hpp"
#include "LogbookPlace.hpp"
#include "LogbookBuilder.hpp"
#include "Logger.hpp"
#include "GRecord.hpp"
#include "Version.hpp"
#include "Blackboard/LiveBlackboard.hpp"
#include "NMEA/MoreData.hpp"
#include "Computer/Settings.hpp"
#include "UISettings.hpp"
#include "LogFile.hpp"

GlueLogbook::GlueLogbook(LiveBlackboard &_blackboard, Path _path,
                         const Waypoints *_waypoints,
                         const Logger *_igc_logger) noexcept
  :blackboard(_blackboard), waypoints(_waypoints), igc_logger(_igc_logger),
   path(_path)
{
  blackboard.AddListener(*this);
}

GlueLogbook::~GlueLogbook() noexcept
{
  blackboard.RemoveListener(*this);
}

void
GlueLogbook::OnLogbookTakeoff(LogbookEntry &entry) noexcept
{
  const ComputerSettings &settings = blackboard.GetComputerSettings();

  entry.pilot = settings.logger.pilot_name.c_str();
  entry.copilot = settings.logger.copilot_name.c_str();
  entry.aircraft = settings.plane.type.c_str();
  entry.registration = settings.plane.registration.c_str();
  entry.competition_id = settings.plane.competition_id.c_str();
}

std::string
GlueLogbook::FindLogbookPlace(const GeoPoint &location) noexcept
{
  return Logbook::FindPlace(waypoints, location,
                            blackboard.GetUISettings().format.coordinate_format);
}

std::optional<double>
GlueLogbook::GetLogbookAirfieldElevation(const GeoPoint &location) noexcept
{
  return Logbook::FindAirfieldElevation(waypoints, location);
}

std::string
GlueLogbook::GetLogbookFile() noexcept
{
  if (igc_logger != nullptr)
    if (const auto igc = igc_logger->GetActivePath(); igc != nullptr)
      return igc.GetBase().c_str();

  return {};
}

void
GlueLogbook::FillLogbookRecorder(LogbookEntry &entry) noexcept
{
  /* the log file is the one this program's IGC logger writes; these
     are the values IGCWriter::WriteHeader() puts into it */
  entry.recorder_code = XCSOAR_IGC_CODE;
  entry.recorder_type = std::string{"XCSOAR,XCSOAR "} + XCSoar_VersionStringOld;
}

void
GlueLogbook::OnLogbookFlight(const LogbookEntry &entry) noexcept
{
  try {
    /* the flight may be in the log book already, read at startup from
       the file of a recording that was interrupted (a restart in
       flight); then the better entry stays */
    Logbook::MergeIntoFile(path, {entry});
  } catch (...) {
    LogError(std::current_exception(), "Failed to write the log book");
  }
}

void
GlueLogbook::OnCalculatedUpdate(const MoreData &basic,
                                const DerivedInfo &calculated)
{
  /* like flights.log: a replay or the simulator is not a flight of
     the pilot */
  if (basic.gps.replay || basic.gps.simulator)
    return;

  recorder.Update(basic, calculated);
}
