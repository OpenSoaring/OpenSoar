// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef HAVE_REMOTE_STICK

#include "RemoteStick.hpp"
#include "RotaryPanel.hpp"
#include "Device/Driver.hpp"

// ------------- SteFly RemoteStick -----------------------------------

static Device *
RemoteStickCreateOnPort(const DeviceConfig &config, Port &com_port)
{
  return new StickRemoteControl(config, com_port);
}

const struct DeviceRegister remote_stick_driver = {
  "RemoteStick",
  "RemoteStick",
  DeviceRegister::MANAGE |
  DeviceRegister::NO_TIMEOUT |
  DeviceRegister::SEND_SETTINGS |
  DeviceRegister::RECEIVE_SETTINGS,
  RemoteStickCreateOnPort,
};

// ------------- SteFly RotaryPanel -----------------------------------
//
// Same SteFly NMEA family as the RemoteStick, identified by its own
// USB ids (VID 0x1209, PID 0x8502). Listed as a separate driver entry
// so the user can pick "SteFly RotaryPanel" explicitly in the port /
// driver dialog.

static Device *
RotaryPanelCreateOnPort(const DeviceConfig &config, Port &com_port)
{
  return new RotaryPanelDevice(config, com_port);
}

const struct DeviceRegister rotary_panel_driver = {
  "RotaryPanel",
  "SteFly RotaryPanel",
  DeviceRegister::MANAGE |
  DeviceRegister::NO_TIMEOUT |
  DeviceRegister::SEND_SETTINGS |
  DeviceRegister::RECEIVE_SETTINGS,
  RotaryPanelCreateOnPort,
};

#endif // HAVE_REMOTE_STICK
