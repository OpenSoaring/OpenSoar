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
 * heading - the true heading are fed into the replay; everything
 * else (vario, wind, circling) is computed by XCSoar's own
 * computers, as with any other replay.
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
    float voltage;
    bool available;
  } sensor{};

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
   * Read records until the next GNSS fix; keeps the latest sensor
   * record on the side.
   *
   * @return false on end-of-file or corrupt data
   */
  bool ReadFix(NMEAInfo &data) noexcept;
};
