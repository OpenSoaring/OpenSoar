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
RotaryPanelDevice::WriteDeviceSettings(const RotaryPanelSettings &new_settings,
                                       OperationEnvironment &env)
{
  // Diff against the cached snapshot, write only what changed; USER
  // is a state the firmware reports and can never be sent back.
  if (new_settings.layout != settings.layout &&
      new_settings.layout != Layout::USER)
    WriteLayout(new_settings.layout, env);

  // Apply optimistically — the firmware does not ack writes.
  const std::lock_guard lock{settings_block.mutex};
  settings = new_settings;
}

void
RotaryPanelDevice::ApplySettingsField(std::string_view key,
                                      std::string_view value)
{
  // Caller holds settings_block.mutex; unknown keys are ignored to
  // keep the driver forward-compatible.
  if (key == LAYOUT_NAME)
    ParseLayout(value, settings.layout);
}

#endif // HAVE_REMOTE_STICK
