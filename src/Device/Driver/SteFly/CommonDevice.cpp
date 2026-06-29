// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#ifdef HAVE_REMOTE_STICK

#include "CommonDevice.hpp"
#include "NMEA/Checksum.hpp"
#include "NMEA/InputLine.hpp"
#include "LogFile.hpp"

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
  } else if (key == "SerialNumber") {
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

  {
    const std::lock_guard lock{info_block.mutex};
    ApplyInfoField(key, value);
    info.available = true;
  }
  return true;
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
    ApplySettingsField(key, value);
  }
  return true;
}

// ============== Shared commands =====================================

void
SteFlyDevice::Restart(OperationEnvironment &env)
{
  // Control group: "$PSRCC,S,Reboot*XX"
  SendNMEA("PSRCC,S,Reboot", env);
}

#endif // HAVE_REMOTE_STICK
