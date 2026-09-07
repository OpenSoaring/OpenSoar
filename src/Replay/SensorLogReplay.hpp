// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "AbstractReplay.hpp"
#include "io/FileReader.hxx"
#include "io/BufferedReader.hxx"
#include "time/BrokenDateTime.hpp"

class Path;

/**
 * Replays a flight sensor log in the LRSX format: a stream of
 * length-tagged binary records holding raw sensor data (IMU,
 * pressures, supply voltage at 100 Hz) and GNSS fixes (10 Hz).
 * Position, time, pressures, g load, voltage and - with a D-GNSS
 * heading - the true heading are fed into the replay, and from
 * heading, airspeed and ground vector the instantaneous and average
 * wind are estimated; everything else (vario, circling) is computed
 * by XCSoar's own computers, as with any other replay.
 */
class SensorLogReplay : public AbstractReplay
{
  FileReader file;
  BufferedReader reader;

  uint64_t file_size = 0, consumed = 0;

  /** the latest raw sensor record (id 20) */
  struct {
    float acceleration[3];
    float pitot_pressure, static_pressure;
    float temperature;
    float voltage;
    bool available;
  } sensor{};

  /**
   * The running average of the estimated wind (north/east
   * components [m/s]), a low-pass over the instantaneous estimates.
   */
  struct {
    double north, east;
    double last_time;
    bool valid;
  } avg_wind{};

  BrokenDate date;

public:
  explicit SensorLogReplay(Path path);
  ~SensorLogReplay() override;

  bool Update(NMEAInfo &data) override;

  double GetProgress() const noexcept override {
    return file_size > 0 ? double(consumed) / double(file_size) : -1;
  }

private:
  bool SkipBytes(std::size_t n) noexcept;

  /**
   * Estimate the winds from the raw data, like the sensor's own
   * firmware shows them in flight: the instantaneous wind is the
   * GNSS ground vector minus the airspeed vector along the true
   * heading, the average is a low-pass over it.
   *
   * @param vn,ve the GNSS ground velocity [m/s]
   * @param heading_rad the D-GNSS true heading [radians]
   */
  void EstimateWind(NMEAInfo &data, double vn, double ve,
                    double heading_rad) noexcept;

  /**
   * Read records until the next GNSS fix; keeps the latest sensor
   * record on the side.
   *
   * @return false on end-of-file or corrupt data
   */
  bool ReadFix(NMEAInfo &data) noexcept;
};
