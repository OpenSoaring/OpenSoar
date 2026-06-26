// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef HAVE_REMOTE_STICK

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
#include "time/TimeoutClock.hpp"
#ifdef _WIN32
# include "Device/Port/SerialPort.hpp"
#else
# include "Device/Port/TTYPort.hpp"
#endif

#include <string>
#include <span>

// PSRCI,R: (S)tick(R)emote(C)ontrol(I)nfo, (R)emote

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

void
StickRemoteControl::Send(const char *sentence, OperationEnvironment &env)
{
  assert(sentence != nullptr);

  PortWriteNMEA(port, sentence, env);
}

bool
StickRemoteControl::Receive(const char *prefix, char *buffer, size_t length,
  OperationEnvironment &env,
  std::chrono::steady_clock::duration _timeout)
{
  assert(prefix != nullptr);


  TimeoutClock timeout(_timeout);

  port.ExpectString(prefix, env, _timeout);

  char *p = (char *)buffer, *end = p + length;
  while (true) {
    size_t nbytes = port.WaitAndRead(std::as_writable_bytes(std::span{ p, std::size_t(end - p) }),
      env, timeout);

    char *q = (char *)memchr(p, '*', nbytes);
    if (q != nullptr) {
      /* stop at checksum */
      *q = 0;
      return true;
    }

    p += nbytes;

    if (p >= end)
      /* line too long */
      return false;
  }
}

void
StickRemoteControl::Restart(OperationEnvironment &env)
{
  Send("POPSQ,Reboot", env);
}

static Device *
RemoteStickCreateOnPort(const DeviceConfig &config, Port &com_port)
{
  return new StickRemoteControl(config, com_port);
}

const struct DeviceRegister remote_stick_driver = {
  "RemoteStick",
  "RemoteStick",
  DeviceRegister::MANAGE |
    // DeviceRegister::NMEA_OUT | // sends all NMEA input to NMEA out
  DeviceRegister::NO_TIMEOUT |
  DeviceRegister::SEND_SETTINGS |
  DeviceRegister::RECEIVE_SETTINGS,
  RemoteStickCreateOnPort,
};

#endif  // HAVE_REMOTE_STICK
