// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Device/Driver/RemoteStick.hpp"
#include "Device/Driver.hpp"

#include "NMEA/Checksum.hpp"
#include "NMEA/InputLine.hpp"
//#include "NMEA/Info.hpp"
//#include "NMEA/Derived.hpp"
#include "LogFile.hpp"

#include <string>

// PSRCI,R: (S)tick(R)emote(C)ontrol(I)nfo, (R)emote

// using std::string_view_literals::operator""sv;

class StickRemoteControl : public AbstractDevice {
  const DeviceConfig &config;
  Port &port;
//  [[maybe_unused]] TODO: 

public:
  StickRemoteControl(const DeviceConfig &_config, Port &_port)
    : config(_config),port(_port) {  }

  /* virtual methods from class Device */
  bool ParseNMEA(const char *line, NMEAInfo &info) override;
};

bool
StickRemoteControl::ParseNMEA(const char *_line,
  [[maybe_unused]] NMEAInfo &info)
{
  // PSRCI,R: (S)tick(R)emote(C)ontrol(I)nfo, (R)emote
  if (!VerifyNMEAChecksum(_line))
    return false;

  NMEAInputLine line(_line);
  const auto type = line.ReadView();
  if (!type.starts_with("$PSRC"))
    return false;

  switch (type[5]) {
    case 'I':  // Info message from RemoteStick
    {
      const auto info_type = line.ReadOneChar();
      if (info_type == 'R') {
        const auto remote_info = line.ReadView();
        LogFmt("RemoteStick Info: {}", remote_info.data());
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

static Device *
RemoteStickCreateOnPort(const DeviceConfig &config, Port &com_port)
{
  return new StickRemoteControl(config, com_port);
}


const struct DeviceRegister remote_stick_driver = {
  "RemoteStick",
  "RemoteStick",
  // DeviceRegister::NMEA_OUT |
  DeviceRegister::NO_TIMEOUT |
  DeviceRegister::SEND_SETTINGS |
  DeviceRegister::RECEIVE_SETTINGS,
  RemoteStickCreateOnPort,
};

