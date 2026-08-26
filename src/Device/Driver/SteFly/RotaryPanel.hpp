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
 * allocation). The OpenSoar UsbSerialHelper picks this device up as a
 * separate USB-Serial endpoint; the user explicitly selects the
 * "SteFly RotaryPanel" driver entry in the device list.
 *
 * Settings are not finalised yet — the firmware side is still in
 * progress. This class therefore lands as a scaffold:
 *   - inherits the full SteFly Info / Reboot / Settings-block plumbing
 *     for free,
 *   - exposes an empty RotaryPanelSettings struct that gets filled in
 *     once the firmware fields are nailed down,
 *   - ApplySettingsField is an empty stub. As soon as the protocol is
 *     set, extending it follows the same 4-step pattern documented in
 *     the RemoteStick driver.
 */
class RotaryPanelDevice final : public SteFlyDevice {
public:
  struct RotaryPanelSettings {
    // TODO(stefly): wire up the actual rotary panel settings here.
    // Likely candidates (placeholder names):
    //   uint32_t rotary_left_mode  = 0;
    //   uint32_t rotary_right_mode = 0;
    //   uint32_t click_action      = 0;
    //
    // For each setting, add a `static constexpr const char *NAME = "…";`
    // identifier — that's the key the firmware will emit in
    // "$PSRCS,R,<NAME>,<Value>*XX" (key and value as two NMEA fields).
  };

private:
  // Protected by settings_block.mutex (from the base class).
  RotaryPanelSettings settings;

public:
  RotaryPanelDevice(const DeviceConfig &cfg, Port &port) noexcept;

  [[gnu::pure]]
  RotaryPanelSettings GetSettings() noexcept;

  /** Diff-and-send the rotary-panel-specific settings.
   *  Skeleton: currently a no-op until the protocol is defined. */
  void WriteDeviceSettings(const RotaryPanelSettings &new_settings,
                           OperationEnvironment &env);

protected:
  /* virtual from SteFlyDevice */
  void ApplySettingsField(std::string_view key,
                          std::string_view value) override;
};

extern const struct DeviceRegister rotary_panel_driver;
