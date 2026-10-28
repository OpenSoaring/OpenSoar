// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "Device/ManagedDevice.hpp"

#include <string>
#include <string_view>
#include <vector>

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
 *   $PSRCI,A,Layout,<n>,<name>*XX  one entry of the layout table
 *   $PSRCI,A,Ready*XX          info block terminator
 *   $PSRCS,A,<Key>,<Value>*XX  setting field (device-specific keys)
 *   $PSRCS,A,Ready*XX          settings block terminator
 *   $PSRCI,I,<free text>*XX    unsolicited info / log line
 *
 * Key and Value are separate NMEA fields (comma-separated), so the
 * driver reads them as two consecutive line.ReadView() calls.
 *
 * User settings: since SteFlyRemoteCore 1.1 every device additionally
 * lists its tuning values in its settings block, after "Layout":
 *
 *   $PSRCS,A,DblClick,<ms>      double click window      (100..1000)
 *   $PSRCS,A,LongPress,<ms>     long press time          (300..1900)
 *   $PSRCS,A,MouseStep,<n>      mouse distance in quarters of the
 *                               layout's distance        (1..16)
 *
 * Core 1.1 also listed "MouseAccel", a multiplier of the mouse step when
 * the input is used fast or long.  Since 1.2 the acceleration is fixed at
 * 1 and no longer a setting, because it confused more than it helped; a
 * MouseAccel field of older firmware is ignored here.
 *
 * They are written with "$PSRCS,S,<Key>,<Value>*XX"; the firmware clamps
 * the value, stores it in its EEPROM and answers with the value in
 * effect. They are the same on every SteFly device and are therefore
 * handled here, in the base class. Older firmware does not send them, so
 * UserSettings::available stays false and the dialog hides them.
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
  /**
   * One key layout the device has: the index "$PSRCS,S,Layout,N"
   * selects and the name to show for it.
   */
  struct LayoutChoice {
    unsigned index;
    std::string name;
  };

  struct SteFlyInfo {
    std::string name;           // firmware device name"
    std::string version;        // firmware version, e.g. "1.4.3"
    std::string file_name;      // currently loaded key-config file
    std::string serial_number;  // SteFly serial (from EEPROM)

    /**
     * The layout table of the device, in the order it was reported.
     * Firmware with SteFlyRemoteCore 1.2 or later sends it in the info
     * block ("$PSRCI,A,Layout,<n>,<name>"), because every device may
     * have other layouts.  It is empty with older firmware, which
     * then cannot have its layout chosen; that needs a firmware update.
     */
    std::vector<LayoutChoice> layouts;

    bool available = false;     // true once at least one field arrived
  };

  /**
   * A key layout of the SteFly firmware: the index "$PSRCS,S,Layout,N"
   * selects.  Which layouts a device has, and under which index, only
   * the device knows; it reports them in SteFlyInfo::layouts.  BASIC is
   * the factory default every device has at index 0.  USER is what the
   * firmware reports when the active key map matches no layout; it can
   * never be selected.
   */
  enum class Layout : uint32_t {
    BASIC = 0,
    USER  = 255,
  };

  static constexpr const char *LAYOUT_NAME = "Layout";

  /**
   * Tuning values common to all SteFly devices (see above).
   */
  struct UserSettings {
    unsigned dblclick_ms = 350;
    unsigned long_press_ms = 900;
    unsigned mouse_step = 4;
    /** true once the firmware has reported at least one of them */
    bool available = false;

    static constexpr const char *DBLCLICK_NAME    = "DblClick";
    static constexpr const char *LONGPRESS_NAME   = "LongPress";
    static constexpr const char *MOUSESTEP_NAME   = "MouseStep";

    static constexpr unsigned DBLCLICK_MIN = 100, DBLCLICK_MAX = 1000;
    static constexpr unsigned LONGPRESS_MIN = 300, LONGPRESS_MAX = 1900;
    static constexpr unsigned MOUSESTEP_MIN = 1, MOUSESTEP_MAX = 16;
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

  // Protected by settings_block.mutex.
  UserSettings user_settings;

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

  /** Snapshot of the common user settings gathered so far. */
  [[gnu::pure]]
  UserSettings GetUserSettings() noexcept;

  /** Send the user settings that differ from the cached snapshot. The
   *  firmware answers each with the value in effect, which updates the
   *  cache; until then the cache is updated optimistically. */
  void WriteUserSettings(const UserSettings &new_settings,
                         OperationEnvironment &env);

  // ============== Shared commands ====================================

  /** Reboot the firmware (USB will re-enumerate). */
  void Restart(OperationEnvironment &env);

protected:
  /** Select a key layout preset ("$PSRCS,S,Layout,<n>"). */
  void WriteLayout(Layout layout, OperationEnvironment &env);

  /** Parse the value of a "Layout" settings field. */
  static bool ParseLayout(std::string_view value, Layout &layout) noexcept;

  /**
   * Apply an info field ("Device", "Version", "FileName", "S/N").
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
  /**
   * Apply a common user setting field. Returns false if the key is
   * not one of them. Called with settings_block.mutex held.
   */
  bool ApplyUserSettingField(std::string_view key, std::string_view value);

  /**
   * Store one entry of the layout table ("Layout,<n>,<name>").
   * Called with info_block.mutex held.
   */
  void ApplyLayoutChoice(std::string_view index, std::string_view name);

  bool ParseInfoSentence(NMEAInputLine &line);
  bool ParseSettingsSentence(NMEAInputLine &line);
};
