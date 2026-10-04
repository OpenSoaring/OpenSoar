// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LogbookReplay.hpp"
#include "Computer/Settings.hpp"
#include "Device/Driver.hpp"
#include "Device/Config.hpp"
#include "Device/Port/NullPort.hpp"
#include "IGC/IGCParser.hpp"
#include "IGC/IGCFix.hpp"
#include "Units/System.hpp"
#include "system/Path.hpp"
#include "util/StringCompare.hxx"

#include <cstring>

/* a driver parses sentences only; it never writes to the port, and
   all drivers can share this one */
static DeviceConfig replay_config;
static NullPort replay_port;

bool
LogbookReplay::IsIgc(Path path) noexcept
{
  return StringEndsWithIgnoreCase(path.c_str(), ".igc");
}

LogbookReplay::LogbookReplay(Path path, const DeviceRegister *driver)
  :reader(path), igc(IsIgc(path)),
   device(!igc && driver != nullptr && driver->CreateOnPort != nullptr
          ? driver->CreateOnPort(replay_config, replay_port)
          : nullptr)
{
  extensions.clear();
  raw_basic.Reset();
  computed_basic.Reset();
  last_basic.Reset();
  calculated.Reset();
  flying_computer.Reset();
  wrap_clock.Reset();
  clock.Reset();
}

LogbookReplay::~LogbookReplay() noexcept = default;

bool
LogbookReplay::Next()
{
  last_basic = computed_basic;

  if (igc ? NextIgc() : NextNmea())
    return true;

  if (computed_basic.time_available)
    flying_computer.Finish(calculated.flight, computed_basic.time);

  return false;
}

bool
LogbookReplay::NextIgc()
{
  const char *line;
  while ((line = reader.ReadLine()) != nullptr) {
    if (line[0] == 'B') {
      IGCFix fix;
      if (IGCParseFix(line, extensions, fix)) {
        CopyFromFix(fix);
        Compute();
        return true;
      }
    } else if (line[0] == 'H') {
      BrokenDate date;
      if (memcmp(line, "HFDTE", 5) == 0 &&
          IGCParseDateRecord(line, date)) {
        (BrokenDate &)raw_basic.date_time_utc = date;
        raw_basic.time_available.Clear();
      }
    } else if (line[0] == 'I') {
      IGCParseExtensions(line, extensions);
    }
  }

  return false;
}

bool
LogbookReplay::NextNmea()
{
  const char *line;
  while ((line = reader.ReadLine()) != nullptr) {
    raw_basic.clock = clock.NextClock(raw_basic.time_available
                                      ? raw_basic.time
                                      : TimeStamp::Undefined());

    if (!device || !device->ParseNMEA(line, raw_basic))
      parser.ParseLine(line, raw_basic);

    if (raw_basic.location_available != last_basic.location_available) {
      Compute();
      return true;
    }
  }

  return false;
}

void
LogbookReplay::CopyFromFix(const IGCFix &fix) noexcept
{
  NMEAInfo &basic = raw_basic;

  if (basic.time_available && basic.date_time_utc.hour >= 23 &&
      fix.time.hour == 0)
    /* midnight roll-over */
    basic.date_time_utc.IncrementDay();

  basic.clock = basic.time = TimeStamp{fix.time.DurationSinceMidnight()};
  basic.time_available.Update(basic.clock);
  basic.date_time_utc.hour = fix.time.hour;
  basic.date_time_utc.minute = fix.time.minute;
  basic.date_time_utc.second = fix.time.second;
  basic.alive.Update(basic.clock);
  basic.location = fix.location;

  if (fix.gps_valid) {
    basic.location_available.Update(basic.clock);
    basic.gps_altitude = fix.gps_altitude;
    basic.gps_altitude_available.Update(basic.clock);
  } else {
    basic.location_available.Clear();
    basic.gps_altitude_available.Clear();
  }

  if (fix.gps_ellipsoid_altitude_available) {
    basic.gps_ellipsoid_altitude = fix.gps_ellipsoid_altitude;
    basic.gps_ellipsoid_altitude_available.Update(basic.clock);
  } else
    basic.gps_ellipsoid_altitude_available.Clear();

  if (fix.pressure_altitude != 0) {
    basic.pressure_altitude = fix.pressure_altitude;
    basic.pressure_altitude_available.Update(basic.clock);
  }

  if (fix.enl >= 0) {
    basic.engine_noise_level = fix.enl;
    basic.engine_noise_level_available.Update(basic.clock);
  }

  if (fix.trt >= 0) {
    basic.track = Angle::Degrees(fix.trt);
    basic.track_available.Update(basic.clock);
  }

  if (fix.gsp >= 0) {
    basic.ground_speed = Units::ToSysUnit(fix.gsp, Unit::KILOMETER_PER_HOUR);
    basic.ground_speed_available.Update(basic.clock);
  }

  if (fix.ias >= 0) {
    auto ias = Units::ToSysUnit(fix.ias, Unit::KILOMETER_PER_HOUR);
    if (fix.tas >= 0)
      basic.ProvideBothAirspeeds(ias,
                                 Units::ToSysUnit(fix.tas,
                                                  Unit::KILOMETER_PER_HOUR));
    else
      basic.ProvideIndicatedAirspeedWithAltitude(ias, basic.pressure_altitude);
  } else if (fix.tas >= 0)
    basic.ProvideTrueAirspeed(Units::ToSysUnit(fix.tas,
                                               Unit::KILOMETER_PER_HOUR));

  if (fix.siu >= 0) {
    basic.gps.satellites_used = fix.siu;
    basic.gps.satellites_used_available.Update(basic.clock);
  }
}

void
LogbookReplay::Compute() noexcept
{
  computed_basic.Reset();
  (NMEAInfo &)computed_basic = raw_basic;
  wrap_clock.Normalise(computed_basic);

  FeaturesSettings features;
  features.nav_baro_altitude_enabled = true;
  computer.Fill(computed_basic, qnh, features);

  computer.Compute(computed_basic, last_basic, last_basic, calculated,
                   ComputerSettings{.polar = {.glide_polar_task = glide_polar}});
  flying_computer.Compute(glide_polar.GetVTakeoff(),
                          computed_basic, calculated,
                          calculated.flight);
}
