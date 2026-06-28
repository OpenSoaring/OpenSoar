// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef HAVE_REMOTE_STICK

#include "RotaryPanel.hpp"

RotaryPanelDevice::RotaryPanelDevice([[maybe_unused]] const DeviceConfig &cfg,
                                     Port &_port) noexcept
  :SteFlyDevice(_port)
{
}

RotaryPanelDevice::RotaryPanelSettings
RotaryPanelDevice::GetSettings() noexcept
{
  const std::lock_guard lock{settings_block.mutex};
  return settings;
}

void
RotaryPanelDevice::WriteDeviceSettings(
  [[maybe_unused]] const RotaryPanelSettings &new_settings,
  [[maybe_unused]] OperationEnvironment &env)
{
  // TODO(stefly): once RotaryPanelSettings has real fields, diff them
  // against `settings` and call WriteDeviceSetting(WRITE_TALKER, NAME,
  // value, env) for each change — same pattern as RemoteStick.cpp.
  //
  // Then apply optimistically:
  //   const std::lock_guard lock{settings_block.mutex};
  //   settings = new_settings;
}

void
RotaryPanelDevice::ApplySettingsField(
  [[maybe_unused]] std::string_view key,
  [[maybe_unused]] std::string_view value)
{
  // Caller holds settings_block.mutex.
  //
  // TODO(stefly): map the firmware's "$PSRCS,R,<Key>,<Value>*XX" reply
  // fields onto RotaryPanelSettings — see RemoteStick.cpp for the
  // shape (each `if (key == NAME) settings.<member> = parse(value);`).
}

#endif // HAVE_REMOTE_STICK
