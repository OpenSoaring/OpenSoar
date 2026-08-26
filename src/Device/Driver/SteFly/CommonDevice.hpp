// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Device/ManagedDevice.hpp"

#include <string>
#include <string_view>

struct NMEAInfo;
class NMEAInputLine;

/**
 * Common base for SteFly Arduino-based devices (RemoteStick,
 * RotaryPanel and any future siblings).
 *
 * Shared protocol surface (the parts identical across devices).
 *
 * Every sentence has the shape  "$PSRC<Group>,<Direction>,…*XX"
 *
 *   Group:    I = Info     S = Settings    C = Control
 *             H = Health   P = Pin config  K = Key remap   …
 *   Direction (host -> device):  R = Request   S = Set
 *   Direction (device -> host):  A = Answer    I = Info (unsolicited)
 *
 *   --- Host -> Stick / Panel ------------------------------------
 *   $PSRCI,R,Info*XX           request the read-only info block
 *   $PSRCS,R,Settings*XX       request the editable settings block
 *   $PSRCS,S,<Key>,<Value>*XX  write one setting (e.g. Layout,2)
 *   $PSRCC,S,Reboot*XX         reboot the firmware
 *
 *   --- Stick / Panel -> Host ------------------------------------
 *   $PSRCI,A,<Key>,<Value>*XX  info field   (Version, FileName, …)
 *   $PSRCI,A,Ready*XX          info block terminator
 *   $PSRCS,A,<Key>,<Value>*XX  setting field (device-specific keys)
 *   $PSRCS,A,Ready*XX          settings block terminator
 *   $PSRCI,I,<free text>*XX    unsolicited info / log line
 *
 * Key and Value are separate NMEA fields (comma-separated), so the
 * driver reads them as two consecutive line.ReadView() calls.
 *
 * Each block has its own ApplyXField hook on the base class:
 *   - ApplyInfoField has a default impl that claims the common info
 *     fields. Subclasses override + chain to the base if they add
 *     more info keys.
 *   - ApplySettingsField is pure virtual — every subclass populates
 *     its own Settings struct here.
 */
class SteFlyDevice : public ManagedDevice {
public:
  /**
   * Static device info — populated once after detection. Common to
   * every SteFly device, so it lives here.
   */
  struct SteFlyInfo {
    std::string name;           // firmware device name"
    std::string version;        // firmware version, e.g. "1.4.3"
    std::string file_name;      // currently loaded key-config file
    std::string serial_number;  // SteFly serial (from EEPROM)
    bool available = false;     // true once at least one field arrived
  };

protected:
  // Per-group talker names. The host emits all sentences as
  // "$PSRC<G>,<direction>,…*XX", so each group has its own talker
  // string. Subclasses pick the right one when writing settings.
  static constexpr const char *INFO_TALKER     = "PSRCI";
  static constexpr const char *SETTINGS_TALKER = "PSRCS";
  static constexpr const char *CONTROL_TALKER  = "PSRCC";

  PendingBlock info_block;
  PendingBlock settings_block;

  // Protected by info_block.mutex.
  SteFlyInfo info;

  explicit SteFlyDevice(Port &port) noexcept;

public:
  /* virtual methods from AbstractDevice */
  bool ParseNMEA(const char *line, NMEAInfo &info) override;
  void LinkTimeout() override;

  // ============== Read-only info block ===============================

  /** Ask the device to dump its info block. Returns immediately. */
  void RequestInfo(OperationEnvironment &env);

  /** Block until the info block's "Ready" arrives or the timeout fires. */
  bool WaitForInfo(unsigned timeout_ms);

  /** Snapshot of the info fields gathered so far. */
  [[gnu::pure]]
  SteFlyInfo GetInfo() noexcept;

  // ============== Editable settings block ============================

  /** Ask the device to dump its settings block. Returns immediately. */
  void RequestSettings(OperationEnvironment &env);

  /** Block until the settings block's "Ready" arrives or the timeout fires. */
  bool WaitForSettings(unsigned timeout_ms);

  // ============== Shared commands ====================================

  /** Reboot the firmware (USB will re-enumerate). */
  void Restart(OperationEnvironment &env);

protected:
  /**
   * Apply an info field ("Device", "Version", "FileName", "SerialNumber").
   * Default implementation handles the common keys; override and
   * chain to the base if your device adds further info fields.
   *
   * Called with info_block.mutex held.
   */
  virtual void ApplyInfoField(std::string_view key, std::string_view value);

  /**
   * Apply a device-specific settings field (e.g. "Layout").
   * Subclasses populate their own Settings struct here.
   *
   * Called with settings_block.mutex held.
   */
  virtual void ApplySettingsField(std::string_view key,
                                  std::string_view value) = 0;

private:
  bool ParseInfoSentence(NMEAInputLine &line);
  bool ParseSettingsSentence(NMEAInputLine &line);
};
