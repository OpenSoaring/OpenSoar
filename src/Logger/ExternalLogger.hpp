// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

class DeviceDescriptor;
struct Declaration;
struct Waypoint;

namespace ExternalLogger {
  void Declare(const Declaration &decl, const Waypoint *home);

  /**
   * Caller is responsible for calling DeviceDescriptor::Borrow() and
   * DeviceDescriptor::Return().
   */
  void DownloadFlightFrom(DeviceDescriptor &device);

  /**
   * Download the newest flight of the logger (the one just landed)
   * without asking, and add it to the log book.  Errors are not
   * shown: this runs on its own after the landing.
   *
   * Caller is responsible for calling DeviceDescriptor::Borrow() and
   * DeviceDescriptor::Return().
   *
   * @param recorder the recorder as in the A record ("LXVJNI"), to
   * skip a flight downloaded before; nullptr if unknown
   * @return true if a flight was downloaded
   */
  bool DownloadNewestFlight(DeviceDescriptor &device, const char *recorder);

  /**
   * Download from Recorder 1 or 2 of the logger settings, from the
   * device given there.
   *
   * @param index 0 for Recorder 1, 1 for Recorder 2
   * @param automatic true after the landing: only the newest flight,
   * without questions and without error messages; false: the list of
   * flights, as from the device list
   * @return true if a flight was downloaded (automatic only)
   */
  bool DownloadFromRecorder(unsigned index, bool automatic);

  /**
   * Is the device of Recorder 1 or 2 configured and connected?
   */
  [[gnu::pure]]
  bool IsRecorderReady(unsigned index) noexcept;
}
