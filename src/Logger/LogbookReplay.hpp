// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "NMEA/MoreData.hpp"
#include "NMEA/Derived.hpp"
#include "Computer/BasicComputer.hpp"
#include "Computer/FlyingComputer.hpp"
#include "Device/Parser.hpp"
#include "Engine/GlideSolvers/GlidePolar.hpp"
#include "IGC/IGCExtensions.hpp"
#include "Atmosphere/Pressure.hpp"
#include "io/FileLineReader.hpp"
#include "time/ReplayClock.hpp"
#include "time/WrapClock.hpp"

#include <memory>

class Path;
class Device;
struct DeviceRegister;
struct IGCFix;

/**
 * Reads an IGC file or an NMEA log fix by fix, as fast as possible,
 * and computes what the log book needs from each fix: the basic values
 * (BasicComputer) and the flight state (FlyingComputer).
 *
 * The program uses it to build the log book from the recorded files,
 * and RunLogbook to check the log book against a collection of
 * flights; both must see the same flights, so they share this class.
 * It does what DebugReplayIGC and DebugReplayNMEA of the test programs
 * do, which cannot be part of the program because they come with the
 * command line handling of the tests.
 */
class LogbookReplay {
  FileLineReaderA reader;

  const bool igc;

  /** IGC: the extensions of the I record */
  IGCExtensions extensions;

  /** NMEA: the device driver, if one was given (nullptr = generic) */
  std::unique_ptr<Device> device;
  NMEAParser parser;
  ReplayClock clock;

  GlidePolar glide_polar{1};
  BasicComputer computer;
  FlyingComputer flying_computer;

  /** The values parsed from the file */
  NMEAInfo raw_basic;

  /** #raw_basic with the changes of #BasicComputer */
  MoreData computed_basic;

  /** #computed_basic of the previous fix */
  MoreData last_basic;

  DerivedInfo calculated;
  WrapClock wrap_clock;
  AtmosphericPressure qnh = AtmosphericPressure::Standard();

public:
  /**
   * Open the file; the extension ".igc" (in any case) selects the IGC
   * parser, everything else is read as NMEA.
   *
   * @param driver the driver that parses the device sentences of an
   * NMEA log, nullptr for the generic NMEA parser only
   *
   * Throws on error.
   */
  LogbookReplay(Path path, const DeviceRegister *driver = nullptr);

  ~LogbookReplay() noexcept;

  LogbookReplay(const LogbookReplay &) = delete;
  LogbookReplay &operator=(const LogbookReplay &) = delete;

  [[gnu::pure]]
  static bool IsIgc(Path path) noexcept;

  /**
   * Read up to the next fix.
   *
   * @return false at the end of the file (then the flight state is
   * finished at the last fix)
   */
  bool Next();

  const MoreData &Basic() const noexcept {
    return computed_basic;
  }

  DerivedInfo &SetCalculated() noexcept {
    return calculated;
  }

private:
  bool NextIgc();
  bool NextNmea();
  void CopyFromFix(const IGCFix &fix) noexcept;
  void Compute() noexcept;
};
