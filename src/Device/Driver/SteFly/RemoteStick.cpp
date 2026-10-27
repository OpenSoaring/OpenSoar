// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef HAVE_REMOTE_STICK

#include "RemoteStick.hpp"


StickRemoteControl::StickRemoteControl([[maybe_unused]] const DeviceConfig &cfg,
                                       Port &_port) noexcept
  :SteFlyDevice(_port)
{
}

StickRemoteControl::RemoteStickSettings
StickRemoteControl::GetSettings() noexcept
{
  const std::lock_guard lock{settings_block.mutex};
  return settings;
}

void
StickRemoteControl::WriteDeviceSettings(const RemoteStickSettings &new_settings,
                                        OperationEnvironment &env)
{
  // Diff against the cached snapshot, write only what changed.
  // USER is a read-only state the firmware reports when the active
  // key map doesn't match any preset; we must never send "Layout,255"
  // back to the firmware (it would be a meaningless command).
  if (new_settings.layout != settings.layout &&
      new_settings.layout != RemoteStickSettings::Layout::USER)
    SetLayout(new_settings.layout, env);

  // Apply optimistically — the firmware does not ack writes.
  const std::lock_guard lock{settings_block.mutex};
  settings = new_settings;
}

void
StickRemoteControl::SetLayout(RemoteStickSettings::Layout layout,
                              OperationEnvironment &env)
{
  WriteLayout(layout, env);
}

void
StickRemoteControl::ApplySettingsField(std::string_view key,
                                       std::string_view value)
{
  // Caller holds settings_block.mutex (from the base class).
  // Only react to Stick-specific keys; unknown keys are silently
  // ignored to keep the driver forward-compatible.
  if (key == LAYOUT_NAME)
    ParseLayout(value, settings.layout);
}

#endif // HAVE_REMOTE_STICK
