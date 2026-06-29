// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "CommonDevice.hpp"

#include <cstdint>

/**
 * SteFly RemoteStick — joystick-style 9-button remote (VID 0x2341,
 * PID variable depending on Arduino board variant).
 *
 * Inherits the common SteFly info-block / reboot / settings-block
 * plumbing from SteFlyDevice and adds the Stick-specific settings
 * struct (currently just the active layout / preset).
 */
class StickRemoteControl final : public SteFlyDevice {
public:
  struct RemoteStickSettings {
    /** Key-layout preset, matches firmware's loadPresetFromFlash. */
    enum class Layout : uint32_t {
      BASIC    = 0,
      ADVANCED = 1,
      ANDROID_ = 2,  // ANDROID is defined on target ANDROID platforms
      STARTER  = 3,
    };
    Layout layout = Layout::BASIC;

    static constexpr const char *LAYOUT_NAME = "Layout";
  };

private:
  // Protected by settings_block.mutex.
  RemoteStickSettings settings;

public:
  StickRemoteControl(const DeviceConfig &cfg, Port &port) noexcept;

  // ============== Settings access ====================================

  [[gnu::pure]]
  RemoteStickSettings GetSettings() noexcept;

  /** Send diffs against the cached snapshot. Updates the cache
   *  optimistically (no ack from the firmware). */
  void WriteDeviceSettings(const RemoteStickSettings &new_settings,
                           OperationEnvironment &env);

  // ============== One-shot commands ==================================

  void SetLayout(RemoteStickSettings::Layout layout,
                 OperationEnvironment &env);

protected:
  /* virtual from SteFlyDevice */
  void ApplySettingsField(std::string_view key,
                          std::string_view value) override;
};

extern const struct DeviceRegister remote_stick_driver;
