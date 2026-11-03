// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Replay/NmeaReplay.hpp"
#include "io/LineReader.hpp"
#include "Device/Parser.hpp"
#include "Device/Driver.hpp"
#include "Device/Register.hpp"
#include "Device/Config.hpp"
#include "Device/MultipleDevices.hpp"
#include "NMEA/Info.hpp"
#include "NMEA/Checksum.hpp"
#include "time/Convert.hxx"
#include "util/StringCompare.hxx"
#include "Replay/NmeaReplaySources.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

/**
 * A time field of the form "hhmmss" or "hhmmss.s...", written with as
 * many decimals as the field it replaces had, so that a receiver sees
 * the same format as before.
 */
static std::string
FormatTimeField(const struct tm &tm, unsigned ms, std::string_view old)
{
  const auto dot = old.find('.');
  const std::size_t decimals = dot == old.npos ? 0 : old.size() - dot - 1;

  char buffer[32];
  int n = snprintf(buffer, sizeof(buffer), "%02d%02d%02d",
                   tm.tm_hour, tm.tm_min, tm.tm_sec);
  if (decimals > 0) {
    char fraction[8];
    snprintf(fraction, sizeof(fraction), "%03u", ms);
    n += snprintf(buffer + n, sizeof(buffer) - n, ".%.*s%s",
                  int(std::min<std::size_t>(decimals, 3)), fraction,
                  std::string(decimals > 3 ? decimals - 3 : 0, '0').c_str());
  }

  return std::string(buffer, n);
}

/**
 * Replace time and date in a position sentence ($xxRMC, $xxGGA,
 * $xxGLL, $xxZDA of any GNSS talker) by \a now and write a new
 * checksum.  This is the replay option "current time on the NMEA
 * output": a device behind such a port that checks the age of a fix,
 * or a logger that records it, then takes the replayed flight for
 * live data.  Every other sentence (FLARM, Larus, ...) carries no
 * absolute time and is returned unchanged, as is a sentence that does
 * not have the expected fields.
 */
static std::string
RetimeSentence(const char *line, std::chrono::system_clock::time_point now)
{
  const std::string_view s{line};
  if (s.size() < 7 || s[0] != '$' || s[1] != 'G')
    return std::string{s};

  const std::string_view type = s.substr(3, 3);
  int time_field = -1, date_field = -1;
  bool zda = false;
  if (type == "RMC") {
    time_field = 1;
    date_field = 9;
  } else if (type == "GGA") {
    time_field = 1;
  } else if (type == "GLL") {
    time_field = 5;
  } else if (type == "ZDA") {
    time_field = 1;
    zda = true;
  } else
    return std::string{s};

  /* the payload between '$' and '*' (or the end) */
  const auto star = s.find('*');
  const std::string_view payload = s.substr(1, star == s.npos
                                            ? s.npos : star - 1);

  std::vector<std::string> fields;
  for (std::size_t start = 0;;) {
    const auto comma = payload.find(',', start);
    fields.emplace_back(payload.substr(start, comma == payload.npos
                                       ? payload.npos : comma - start));
    if (comma == payload.npos)
      break;
    start = comma + 1;
  }

  struct tm tm;
  try {
    tm = GmTime(now);
  } catch (...) {
    return std::string{s};
  }

  const unsigned ms = unsigned(std::chrono::duration_cast<std::chrono::milliseconds>(
    now.time_since_epoch()).count() % 1000);

  if (time_field >= int(fields.size()))
    return std::string{s};

  if (!fields[time_field].empty())
    fields[time_field] = FormatTimeField(tm, ms, fields[time_field]);

  char buffer[16];
  if (date_field >= 0 && date_field < int(fields.size()) &&
      !fields[date_field].empty()) {
    snprintf(buffer, sizeof(buffer), "%02d%02d%02d", tm.tm_mday,
             tm.tm_mon + 1, tm.tm_year % 100);
    fields[date_field] = buffer;
  }

  if (zda && fields.size() > 4) {
    snprintf(buffer, sizeof(buffer), "%02d", tm.tm_mday);
    fields[2] = buffer;
    snprintf(buffer, sizeof(buffer), "%02d", tm.tm_mon + 1);
    fields[3] = buffer;
    snprintf(buffer, sizeof(buffer), "%04d", tm.tm_year + 1900);
    fields[4] = buffer;
  }

  std::string body;
  for (std::size_t i = 0; i < fields.size(); ++i) {
    if (i > 0)
      body += ',';
    body += fields[i];
  }

  snprintf(buffer, sizeof(buffer), "*%02X",
           NMEAChecksum(std::string_view{body}));
  return "$" + body + buffer;
}

static bool
IsPositionSentence(const char *line) noexcept
{
  return (StringLength(line) >= 6 && StringStartsWith(line, "$G") &&
          StringStartsWith(line + 3, "RMC")) ||
    StringStartsWith(line, "$FLYSEN");
}

NmeaReplay::Channel::Channel(std::string_view _label) noexcept
  :label(_label)
{
  parser.SetReal(false);
  data.Reset();
}

NmeaReplay::Channel::~Channel() noexcept
{
  delete device;
}

NmeaReplay::NmeaReplay(std::unique_ptr<NLineReader> &&_reader,
                       const DeviceConfig &config,
                       MultipleDevices *_outputs,
                       const bool *_output_enabled,
                       const bool *_output_current_time,
                       NmeaReplaySources *_sources)
  :reader(std::move(_reader)),
   default_driver(config.driver_name.c_str()),
   sources(_sources),
   outputs(_outputs),
   output_enabled(_output_enabled),
   output_current_time(_output_current_time)
{
  if (sources == nullptr) {
    own_sources = std::make_unique<NmeaReplaySources>();
    sources = own_sources.get();
  }

  clock.Reset();
}

NmeaReplay::~NmeaReplay() noexcept = default;

void
NmeaReplay::ScanSources(NLineReader &reader, NmeaReplaySources &sources,
                        unsigned max_lines)
{
  char *buffer;
  for (unsigned i = 0; i < max_lines &&
         (buffer = reader.ReadLine()) != nullptr; ++i) {
    std::string_view label;
    const char *line = NmeaReplaySources::SplitLine(buffer, label);
    if (line == nullptr)
      continue;

    const unsigned rank = sources.FindOrAdd(label);
    if (IsPositionSentence(line))
      sources.SetHasPosition(rank);
  }
}

NmeaReplay::Channel *
NmeaReplay::FindChannel(std::string_view label) noexcept
{
  for (auto &i : channels)
    if (i->label == label)
      return i.get();
  return nullptr;
}

NmeaReplay::Channel &
NmeaReplay::GetChannel(std::string_view label)
{
  if (Channel *c = FindChannel(label))
    return *c;

  auto &channel = *channels.emplace_back(std::make_unique<Channel>(label));

  /* the driver named in the head parses the sentences of its source,
     as it did in flight; lines without a head go to the driver of the
     replay, and a driver this program does not know leaves the
     sentences to the generic parser */
  std::string driver_name{NmeaReplaySources::GetDriverName(label)};
  if (driver_name.empty())
    driver_name = default_driver;

  const struct DeviceRegister *driver =
    FindDriverByName(driver_name.c_str());
  if (driver != nullptr && driver->CreateOnPort != nullptr) {
    DeviceConfig config;
    config.Clear();
    channel.device = driver->CreateOnPort(config, port);
  }

  return channel;
}

bool
NmeaReplay::IsPacing(const Channel &channel) const noexcept
{
  const int i = sources->GetPacingSource();
  return i >= 0 && (*sources)[i].label == channel.label;
}

void
NmeaReplay::ParseLine(Channel &channel, const char *line)
{
  /* the replayed sentences go out on the "NMEA output" ports, as the
     received ones do in flight, so that a device behind such a port (a
     second program, a display) follows the replayed flight */
  if (outputs != nullptr &&
      (output_enabled == nullptr || *output_enabled)) {
    if (output_current_time != nullptr && *output_current_time)
      outputs->ForwardReplayLine(
        RetimeSentence(line, std::chrono::system_clock::now()).c_str());
    else
      outputs->ForwardReplayLine(line);
  }

  now = clock.NextClock(master_time);

  NMEAInfo &data = channel.data;
  data.clock = now;

  if ((channel.device != nullptr && channel.device->ParseNMEA(line, data)) ||
      channel.parser.ParseLine(line, data)) {
    data.gps.replay = true;
    data.alive.Update(data.clock);
  }

  if ((IsPacing(channel) || sources->GetPacingSource() < 0) &&
      data.time_available)
    master_time = data.time;
}

void
NmeaReplay::Merge(NMEAInfo &data) noexcept
{
  NMEAInfo merged;
  merged.Reset();

  for (const auto &source : *sources) {
    if (!source.enabled)
      continue;

    Channel *c = FindChannel(source.label);
    if (c == nullptr || !c->data.alive)
      continue;

    /* let the values of a source that has fallen silent expire, as
       they would in flight */
    c->data.clock = now;
    c->data.Expire();
    merged.Complement(c->data);
  }

  merged.clock = now;
  data = merged;
}

bool
NmeaReplay::Update(NMEAInfo &data)
{
  char *buffer;

  while ((buffer = reader->ReadLine()) != nullptr) {
    /* a line of the NMEA log has a head with time and source in front
       of the sentence ("hhmmss.mmm - A Driver: $..."); cut it off, so
       that logs with and without it replay alike */
    std::string_view label;
    const char *line = NmeaReplaySources::SplitLine(buffer, label);
    if (line == nullptr)
      continue;

    const unsigned rank = sources->FindOrAdd(label);
    if (IsPositionSentence(line))
      sources->SetHasPosition(rank);
    if (!(*sources)[rank].enabled)
      continue;

    Channel &channel = GetChannel(label);
    ParseLine(channel, line);

    /* one step of the replay per position of the leading source;
       the position sentences of the others would only make the
       replay stutter */
    if (IsPositionSentence(line) && IsPacing(channel)) {
      Merge(data);
      return true;
    }
  }

  return false;
}
