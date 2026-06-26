// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

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

#pragma once



class StickRemoteControl : public AbstractDevice {
#ifdef _WIN32
  // config is unused up to now
  [[maybe_unused]] const DeviceConfig &config;
#endif
  Port &port;
  static MessageOperationEnvironment env;
//  UI::PeriodicTimer timer{ [this] { UpdateList(); } };

public:
  StickRemoteControl([[maybe_unused]] const DeviceConfig &_config, Port &_port)
#ifdef _WIN32
    : config(_config), port(_port) {
#else
    : port(_port) {
#endif
    // PortWriteNMEA(port, "PSRCI,Q,Version", env);
    // PortWriteNMEA(port, "PSRCI,Q,Time", env);

    /* < 1sec for WatchDog heartbeat */
    // timer.Schedule(std::chrono::milliseconds(800));
  }

  /* virtual methods from class Device */
  bool ParseNMEA(const char *line, NMEAInfo & info) override;
  void Restart(OperationEnvironment &env);

private:
  PortState state;

  void Send(const char *sentence, OperationEnvironment &env);
  bool Receive(const char *prefix, char *buffer, size_t length,
    OperationEnvironment &env,
    std::chrono::steady_clock::duration timeout);
#ifdef SRC_UPDATE_LIST
  void UpdateList() {
#if defined(__TEST__  )
    LogFmt("USB-Update");
#endif   //defined(__TEST__  )

    if (state != port.GetState())
      state = port.GetState();

    if (state == PortState::READY) {
#if defined(__TEST__  )
      // heartbeat every second for testing
      try {
        PortWriteNMEA(port, "PSRCI,Q,Device", env);
      }
      catch (const std::exception &e) {
        LogFmt("StickRemoteControl: Write exception: {}", e.what());
      }
#endif   //defined(__TEST__  )
    }
  }
#endif
};

extern const struct DeviceRegister remote_stick_driver;
