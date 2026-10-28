// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef HAVE_REMOTE_STICK

#include "CommonDevice.hpp"
#include "NMEA/Checksum.hpp"
#include "NMEA/InputLine.hpp"
#include "LogFile.hpp"

#include <charconv>
#include <chrono>
#include <string>

SteFlyDevice::SteFlyDevice(Port &_port) noexcept
  :ManagedDevice(_port)
{
}

bool
SteFlyDevice::ParseNMEA(const char *_line,
                        [[maybe_unused]] NMEAInfo &nmea_info)
{
  if (!VerifyNMEAChecksum(_line))
    return false;

  NMEAInputLine line(_line);
  const auto type = line.ReadView();

  // Talker family: "$PSRC<X>" where X selects the channel. The
  // SteFly firmware's sendNMEA(<X>, …) emits each section under
  // its own letter — we route Info ("I") and Settings ("S") to
  // their respective parsers.
  if (!type.starts_with("$PSRC") || type.size() < 6)
    return false;

  switch (type[5]) {
  case 'I':
    return ParseInfoSentence(line);
  case 'S':
    return ParseSettingsSentence(line);
  default:
    return false;
  }
}

void
SteFlyDevice::LinkTimeout()
{
  // Wake everything waiting — no more replies coming.
  info_block.NotifyReady();
  settings_block.NotifyReady();
}

// ============== Read-only info block ================================

void
SteFlyDevice::RequestInfo(OperationEnvironment &env)
{
  info_block.Reset();
  {
    /* the device reports its whole layout table with every info
       block; start from an empty one, so that a layout the firmware
       no longer has (after an update) does not stay in the list */
    const std::lock_guard lock{info_block.mutex};
    info.layouts.clear();
  }
  SendNMEA("PSRCI,R,Info", env);
}

bool
SteFlyDevice::WaitForInfo(unsigned timeout_ms)
{
  return info_block.WaitFor(std::chrono::milliseconds(timeout_ms));
}

SteFlyDevice::SteFlyInfo
SteFlyDevice::GetInfo() noexcept
{
  const std::lock_guard lock{info_block.mutex};
  return info;
}

void
SteFlyDevice::ApplyInfoField(std::string_view key, std::string_view value)
{
  // Caller holds info_block.mutex. Default implementation handles
  // the common SteFly info fields. Subclasses may override and
  // chain to the base; unknown keys are silently ignored so the
  // driver stays forward-compatible.
  if (key == "Version") {
    info.version.assign(value);
  } else if (key == "Device") {
    info.name.assign(value);
  } else if (key == "FileName") {
    info.file_name.assign(value);
  } else if (key == "S/N") {
    info.serial_number.assign(value);
  }
}

// ============== Editable settings block =============================

void
SteFlyDevice::RequestSettings(OperationEnvironment &env)
{
  settings_block.Reset();
  SendNMEA("PSRCI,R,Settings", env);
}

bool
SteFlyDevice::WaitForSettings(unsigned timeout_ms)
{
  return settings_block.WaitFor(std::chrono::milliseconds(timeout_ms));
}

// ============== Sentence parsing ====================================

bool
SteFlyDevice::ParseInfoSentence(NMEAInputLine &line)
{
  const auto kind = line.ReadOneChar();
  if (kind == 'I') {
    // Unsolicited info / log message ("$PSRCI,I,Pin updated*XX").
    const auto rest = line.ReadView();
    LogFmt("SteFly Info: {}", std::string(rest).c_str());
    return true;
  }
  if (kind != 'A')
    return false;  // not an answer; ignore other directions

  // After 'A': either standalone "Ready" (block terminator) or a
  // "<Key>,<Value>" pair as two consecutive NMEA fields.
  const auto key = line.ReadView();
  if (key == "Ready") {
    info_block.NotifyReady();
    return true;
  }

  const auto value = line.ReadView();

  if (key == LAYOUT_NAME) {
    /* one entry of the layout table: "Layout,<index>,<name>" */
    const auto name = line.ReadView();
    const std::lock_guard lock{info_block.mutex};
    ApplyLayoutChoice(value, name);
    info.available = true;
    return true;
  }

  {
    const std::lock_guard lock{info_block.mutex};
    ApplyInfoField(key, value);
    info.available = true;
  }
  return true;
}

void
SteFlyDevice::ApplyLayoutChoice(std::string_view index,
                                std::string_view name)
{
  Layout layout;
  if (!ParseLayout(index, layout) || layout == Layout::USER ||
      name.empty())
    return;

  const unsigned i = unsigned(layout);

  /* a repeated index (the table was sent again without a new request)
     replaces the entry instead of adding a second one */
  for (auto &choice : info.layouts) {
    if (choice.index == i) {
      choice.name.assign(name);
      return;
    }
  }

  info.layouts.push_back({i, std::string{name}});
}

bool
SteFlyDevice::ParseSettingsSentence(NMEAInputLine &line)
{
  const auto kind = line.ReadOneChar();
  if (kind != 'A')
    return false;  // only answers carry settings data

  // Same shape as the info block: "<Key>,<Value>" fields with a
  // standalone "Ready" as terminator.
  const auto key = line.ReadView();
  if (key == "Ready") {
    settings_block.NotifyReady();
    return true;
  }

  const auto value = line.ReadView();

  {
    const std::lock_guard lock{settings_block.mutex};
    if (!ApplyUserSettingField(key, value))
      ApplySettingsField(key, value);
  }
  return true;
}

// ============== Common user settings ================================

bool
SteFlyDevice::ApplyUserSettingField(std::string_view key,
                                    std::string_view value)
{
  unsigned *target;
  if (key == UserSettings::DBLCLICK_NAME)
    target = &user_settings.dblclick_ms;
  else if (key == UserSettings::LONGPRESS_NAME)
    target = &user_settings.long_press_ms;
  else if (key == UserSettings::MOUSESTEP_NAME)
    target = &user_settings.mouse_step;
  else if (key == UserSettings::MOUSEACCEL_NAME)
    target = &user_settings.mouse_accel;
  else
    return false;

  unsigned parsed = 0;
  auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(),
                                   parsed);
  if (ec == std::errc{}) {
    *target = parsed;
    user_settings.available = true;
  }
  return true;
}

SteFlyDevice::UserSettings
SteFlyDevice::GetUserSettings() noexcept
{
  const std::lock_guard lock{settings_block.mutex};
  return user_settings;
}

void
SteFlyDevice::WriteUserSettings(const UserSettings &new_settings,
                                OperationEnvironment &env)
{
  const UserSettings old_settings = GetUserSettings();

  // Diff against the cached snapshot, write only what changed: each
  // write is an EEPROM write on the device.
  if (new_settings.dblclick_ms != old_settings.dblclick_ms)
    WriteDeviceSetting(SETTINGS_TALKER, 'S', UserSettings::DBLCLICK_NAME,
                       std::to_string(new_settings.dblclick_ms), env);
  if (new_settings.long_press_ms != old_settings.long_press_ms)
    WriteDeviceSetting(SETTINGS_TALKER, 'S', UserSettings::LONGPRESS_NAME,
                       std::to_string(new_settings.long_press_ms), env);
  if (new_settings.mouse_step != old_settings.mouse_step)
    WriteDeviceSetting(SETTINGS_TALKER, 'S', UserSettings::MOUSESTEP_NAME,
                       std::to_string(new_settings.mouse_step), env);
  if (new_settings.mouse_accel != old_settings.mouse_accel)
    WriteDeviceSetting(SETTINGS_TALKER, 'S', UserSettings::MOUSEACCEL_NAME,
                       std::to_string(new_settings.mouse_accel), env);

  // Apply optimistically; the firmware's answer ($PSRCS,A,<Key>,<v>)
  // corrects the cache if it had to clamp a value.
  const std::lock_guard lock{settings_block.mutex};
  const bool available = user_settings.available;
  user_settings = new_settings;
  user_settings.available = available;
}

// ============== Shared commands =====================================

void
SteFlyDevice::Restart(OperationEnvironment &env)
{
  // Control group: "$PSRCC,S,Reboot*XX"
  SendNMEA("PSRCC,S,Reboot", env);
}

void
SteFlyDevice::WriteLayout(Layout layout, OperationEnvironment &env)
{
  // "$PSRCS,S,Layout,<n>*XX" — Settings group, Set direction.
  WriteDeviceSetting(SETTINGS_TALKER, 'S', LAYOUT_NAME,
                     std::to_string(uint32_t(layout)), env);
}

bool
SteFlyDevice::ParseLayout(std::string_view value, Layout &layout) noexcept
{
  unsigned long parsed = 0;
  auto [ptr, ec] = std::from_chars(value.data(),
                                   value.data() + value.size(),
                                   parsed);
  if (ec != std::errc{})
    return false;

  layout = Layout(parsed);
  return true;
}

#endif // HAVE_REMOTE_STICK
