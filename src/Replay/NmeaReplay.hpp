// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "AbstractReplay.hpp"
#include "time/ReplayClock.hpp"
#include "Device/Port/NullPort.hpp"

#include "Device/Parser.hpp"
#include "NMEA/Info.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

class NLineReader;
class Device;
class MultipleDevices;
class NmeaReplaySources;
struct DeviceConfig;

/**
 * Replays an NMEA log.  Each source of the log (each device, told
 * apart by the head of the line, see #NmeaReplaySources) gets its own
 * parser, driver and data set, and the data sets are merged by rank as
 * the devices are in flight; sources the user has switched off are
 * left out entirely, in the merge as on the "NMEA output" ports.
 */
class NmeaReplay: public AbstractReplay
{
  struct Channel {
    std::string label;
    NMEAParser parser;
    Device *device = nullptr;
    NMEAInfo data;

    explicit Channel(std::string_view _label) noexcept;
    ~Channel() noexcept;

    Channel(const Channel &) = delete;
    Channel &operator=(const Channel &) = delete;
  };

  std::unique_ptr<NLineReader> reader;

  /** the driver for lines without a head */
  const std::string default_driver;

  NullPort port;

  std::vector<std::unique_ptr<Channel>> channels;

  /**
   * The sources in the order of their rank.  Points to the list of
   * the #Replay (so that the choice of the user survives a jump in the
   * file), or to #own_sources.
   */
  NmeaReplaySources *sources;
  std::unique_ptr<NmeaReplaySources> own_sources;

  ReplayClock clock;

  /** the clock value of the last line read */
  TimeStamp now = {};

  /**
   * The latest time sent by the source that paces the replay; it
   * drives the replay clock.  Only one source may do so, because two
   * receivers rarely agree on the second, and a clock that sees the
   * time go back jumps ahead (see ReplayClock::NextClock()).
   */
  TimeStamp master_time = TimeStamp::Undefined();

  /**
   * The devices whose "NMEA output" ports get every replayed sentence,
   * or nullptr.
   */
  MultipleDevices *const outputs;

  /**
   * Points to the replay option "current time on the NMEA output": if
   * true, the time and date in the replayed position sentences are
   * replaced by the current time before they go out (see
   * RetimeSentence()).  Read for every sentence, so that a change in
   * the replay dialog takes effect at once.  nullptr means never.
   */
  const bool *const output_current_time;

public:
  NmeaReplay(std::unique_ptr<NLineReader> &&_reader,
             const DeviceConfig &config,
             MultipleDevices *_outputs=nullptr,
             const bool *_output_current_time=nullptr,
             NmeaReplaySources *_sources=nullptr);
  ~NmeaReplay() noexcept;

  bool Update(NMEAInfo &data) override;

  /**
   * Read the first lines of an NMEA log and enter its sources into
   * the list, so that the replay dialog can show them before they
   * appear in the replay.  Sources that start later are added when
   * the replay reaches them.
   */
  static void ScanSources(NLineReader &reader, NmeaReplaySources &sources,
                          unsigned max_lines);

private:
  Channel &GetChannel(std::string_view label);
  Channel *FindChannel(std::string_view label) noexcept;

  /**
   * Is this the source whose positions pace the replay (see
   * NmeaReplaySources::GetPacingSource())?
   */
  bool IsPacing(const Channel &channel) const noexcept;

  void ParseLine(Channel &channel, const char *line);

  /**
   * Merge the data of the enabled sources by rank into \a data, the
   * way the device blackboard merges the devices in flight.
   */
  void Merge(NMEAInfo &data) noexcept;
};
