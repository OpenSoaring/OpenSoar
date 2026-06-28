// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef HAVE_REMOTE_STICK

#include "RemoteStick.hpp"

#include <charconv>
#include <string>

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
  if (new_settings.layout != settings.layout)
    SetLayout(new_settings.layout, env);

  // Apply optimistically — the firmware does not ack writes.
  const std::lock_guard lock{settings_block.mutex};
  settings = new_settings;
}

void
StickRemoteControl::SetLayout(RemoteStickSettings::Layout layout,
                              OperationEnvironment &env)
{
  // "$POPSQ,Layout,<n>*XX" — confirmed by the firmware author as the
  // command the current firmware still accepts.
  WriteDeviceSetting(WRITE_TALKER,
                     RemoteStickSettings::LAYOUT_NAME,
                     std::to_string(uint32_t(layout)),
                     env);
}

void
StickRemoteControl::ApplySettingsField(std::string_view key,
                                       std::string_view value)
{
  // Caller holds settings_block.mutex (from the base class).
  // Only react to Stick-specific keys; unknown keys are silently
  // ignored to keep the driver forward-compatible.
  if (key == RemoteStickSettings::LAYOUT_NAME) {
    unsigned long parsed = 0;
    auto [ptr, ec] = std::from_chars(value.data(),
                                     value.data() + value.size(),
                                     parsed);
    if (ec == std::errc{})
      settings.layout = RemoteStickSettings::Layout(parsed);
  }
}

#endif // HAVE_REMOTE_STICK
