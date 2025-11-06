// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Device/Driver/RemoteStick.hpp"
#include "Device/Driver.hpp"
#include "Device/Config.hpp"
#include "Device/MultipleDevices.hpp"
#include "BackendComponents.hpp"
#include "Components.hpp"

#include "NMEA/Checksum.hpp"
#include "NMEA/InputLine.hpp"
#include "LogFile.hpp"
#include "ui/event/PeriodicTimer.hpp"
#include "Device/Port/State.hpp"
#include "Device/Util/NMEAWriter.hpp"
#include "Operation/MessageOperationEnvironment.hpp"
#ifdef _WIN32
# include "Device/Port/SerialPort.hpp"
#else
# include "Device/Port/TTYPort.hpp"
#endif

#include <string>
#include <span>

// PSRCI,R: (S)tick(R)emote(C)ontrol(I)nfo, (R)emote

class StickRemoteControl : public AbstractDevice {
#ifdef _WIN32
  // config is unused up to now
  [[maybe_unused]] const DeviceConfig &config;
#endif
  Port &port;
  static MessageOperationEnvironment env;
  UI::PeriodicTimer timer{ [this] { UpdateList(); }};
  
public:
  StickRemoteControl([[maybe_unused]] const DeviceConfig &_config, Port &_port)
#ifdef _WIN32
    : config(_config), port(_port) {
#else
    : port(_port) {
#endif
    // PortWriteNMEA(port, "PSRCI,Q,Version", env);
    PortWriteNMEA(port, "PSRCI,Q,Time", env);
    
    /* < 1sec for WatchDog heartbeat */
    timer.Schedule(std::chrono::milliseconds(800));
  }

  /* virtual methods from class Device */
  bool ParseNMEA(const char *line, NMEAInfo &info) override;

private:
  PortState state;
  void UpdateList() {
    LogFmt("USB-Update");

    if (state != port.GetState())
      state = port.GetState();

    if (state == PortState::READY) {

      try {
        PortWriteNMEA(port, "PSRCI,Q,Device", env);
      } catch (const std::exception &e) {
        LogFmt("StickRemoteControl: Write exception: {}", e.what());
      }
    }
  }
};

MessageOperationEnvironment StickRemoteControl::env;

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
    // DeviceRegister::NMEA_OUT | // sends all NMEA input to NMEA out
  DeviceRegister::NO_TIMEOUT |
  DeviceRegister::SEND_SETTINGS |
  DeviceRegister::RECEIVE_SETTINGS,
  RemoteStickCreateOnPort,
};

