// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "CommonDevice.hpp"

#include <cstdint>

/**
 * SteFly RotaryPanel — Arduino-based control panel with two rotary
 * encoders (each with a push-click) instead of the RemoteStick's
 * joystick buttons.
 *
 * USB identification: VID 0x1209, PID 0x8502 (pid.codes / SteFly
 * allocation). Like the RemoteStick it is found by its USB ids at
 * startup and bound with this driver to a fixed device slot of its
 * own (ROTARY_PORT), which the pilot cannot edit; the device list
 * offers its manage dialog.
 *
 * The panel firmware is built on the same SteFlyRemoteCore library as
 * the RemoteStick, so it speaks the same protocol: the info block with
 * its own table of key layouts, the layout selection and the reboot.
 * Settings of the encoders themselves are
 * not defined yet; once the firmware has them, they are added to
 * RotaryPanelSettings and ApplySettingsField the same way.
 */
class RotaryPanelDevice final : public SteFlyDevice {
public:
  struct RotaryPanelSettings {
    /** Key-layout preset, see SteFlyDevice::Layout. */
    using Layout = SteFlyDevice::Layout;
    Layout layout = Layout::BASIC;
  };

private:
  // Protected by settings_block.mutex (from the base class).
  RotaryPanelSettings settings;

public:
  RotaryPanelDevice(const DeviceConfig &cfg, Port &port) noexcept;

  [[gnu::pure]]
  RotaryPanelSettings GetSettings() noexcept;

  /** Send diffs against the cached snapshot. Updates the cache
   *  optimistically (no ack from the firmware). */
  void WriteDeviceSettings(const RotaryPanelSettings &new_settings,
                           OperationEnvironment &env);

protected:
  /* virtual from SteFlyDevice */
  void ApplySettingsField(std::string_view key,
                          std::string_view value) override;
};

extern const struct DeviceRegister rotary_panel_driver;
