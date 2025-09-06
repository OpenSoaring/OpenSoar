// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Replay/NmeaReplay.hpp"
#include "io/LineReader.hpp"
#include "Device/Parser.hpp"
#include "Device/Driver.hpp"
#include "Device/Register.hpp"
#include "Device/Config.hpp"
#include "Device/Descriptor.hpp"
#include "Device/MultipleDevices.hpp"
#include "NMEA/Info.hpp"
#include "util/StringCompare.hxx"
#include "BackendComponents.hpp"
#include "Components.hpp"

NmeaReplay::NmeaReplay(std::unique_ptr<NLineReader> &&_reader,
                       const DeviceConfig &config)
  :reader(std::move(_reader)),
   parser(new NMEAParser()),
   device(nullptr)
{
  parser->SetReal(false);

  const struct DeviceRegister *driver = FindDriverByName(config.driver_name);
  assert(driver != nullptr);
  if (driver->CreateOnPort != nullptr) {
    DeviceConfig config;
    config.Clear();
    device = driver->CreateOnPort(config, port);
  }

  clock.Reset();
}

NmeaReplay::~NmeaReplay()
{
  delete device;
  delete parser;
}

bool
NmeaReplay::ParseLine(const char *line, NMEAInfo &data)
{
  for (DeviceDescriptor *i : *backend_components->devices) {
    DeviceDescriptor &device = *i;
    if (device.IsDriver("NmeaOut")) {
      device.ForwardLine(line);
    }
  }

  data.clock = clock.NextClock(data.time_available
                               ? data.time
                               : TimeStamp::Undefined());

  if ((device != nullptr && device->ParseNMEA(line, data)) ||
      (parser != nullptr && parser->ParseLine(line, data))) {
    data.gps.replay = true;
    data.alive.Update(data.clock);

    return true;
  } else
    return false;
}

bool
NmeaReplay::ReadUntilRMC(NMEAInfo &data)
{
  char *buffer;

  while ((buffer = reader->ReadLine()) != nullptr) {

    const char *line;
    if (buffer[0] == '$') {
      line = buffer;
    } else {
      line = strstr(buffer, ": $");
      if (line)
        line += 2; // ':' and ' '
    }

    if (line && line[0] == '$') {
      ParseLine(line, data);
      if ((StringLength(line) >= 6 &&
        StringStartsWith(line, "$G") &&
        StringStartsWith(line + 3, "RMC")) ||
        StringStartsWith(line, "$FLYSEN"))
        return true;
    }
  }

  return false;
}

bool
NmeaReplay::Update(NMEAInfo &data)
{
  return ReadUntilRMC(data);
}
