// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "System.hpp"
#include "lib/dbus/Connection.hxx"
#include "lib/dbus/ScopeMatch.hxx"
#include "lib/dbus/Systemd.hxx"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"
#include "io/KeyValueFileReader.hpp"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "io/FileLineReader.hpp"
#include "Dialogs/Error.hpp"
#include "DisplayOrientation.hpp"
#include "Hardware/RotateDisplay.hpp"

#include <unistd.h>
#include <sys/stat.h>
#include <fmt/format.h>

#include <map>

static constexpr const char *boot_config = "/boot/config.uEnv";

bool
IsOpenVario() noexcept
{
  return File::Exists(Path(boot_config));
}

void
LoadConfigFile(std::map<std::string, std::string, std::less<>> &map, Path path)
{
  FileLineReaderA reader(path);
  KeyValueFileReader kvreader(reader);
  KeyValuePair pair;
  while (kvreader.Read(pair))
    map.emplace(pair.key, pair.value);
}

void
WriteConfigFile(std::map<std::string, std::string, std::less<>> &map, Path path)
{
  FileOutputStream file(path);
  BufferedOutputStream buffered(file);

  for (const auto &i : map)
    buffered.Fmt("{}={}\n", i.first, i.second);

  buffered.Flush();
  file.Commit();
}

uint_least8_t
OpenvarioGetBrightness() noexcept
{
  char line[4];
  int result = 10;

  if (File::ReadString(Path("/sys/class/backlight/lcd/brightness"), line, sizeof(line))) {
    result = atoi(line);
  }

  return result;
}

void
OpenvarioSetBrightness(uint_least8_t value) noexcept
{
  if (value < 1) { value = 1; }
  if (value > 10) { value = 10; }

  File::WriteExisting(Path("/sys/class/backlight/lcd/brightness"), fmt::format_int{value}.c_str());
}

DisplayOrientation
OpenvarioGetRotation()
{
  std::map<std::string, std::string, std::less<>> map;
  LoadConfigFile(map, Path(boot_config));

  uint_least8_t result;
  result = map.contains("rotation") ? std::stoi(map.find("rotation")->second) : 0;

  switch (result) {
  case 0: return DisplayOrientation::LANDSCAPE;
  case 1: return DisplayOrientation::REVERSE_PORTRAIT;
  case 2: return DisplayOrientation::REVERSE_LANDSCAPE;
  case 3: return DisplayOrientation::PORTRAIT;
  default: return DisplayOrientation::DEFAULT;
  }
}

/**
 * The value of "rotation" in /boot/config.uEnv, which is also the
 * value of /sys/class/graphics/fbcon/rotate.
 */
static constexpr unsigned
ToConfigRotation(DisplayOrientation orientation) noexcept
{
  switch (orientation) {
  case DisplayOrientation::DEFAULT:
  case DisplayOrientation::LANDSCAPE:
    break;
  case DisplayOrientation::REVERSE_PORTRAIT:
    return 1;
  case DisplayOrientation::REVERSE_LANDSCAPE:
    return 2;
  case DisplayOrientation::PORTRAIT:
    return 3;
  };

  return 0;
}

void
OpenvarioSaveRotation(DisplayOrientation orientation)
{
  std::map<std::string, std::string, std::less<>> map;
  LoadConfigFile(map, Path(boot_config));
  map.insert_or_assign("rotation",
                       fmt::format_int{ToConfigRotation(orientation)}.c_str());
  WriteConfigFile(map, Path(boot_config));
}

void
OpenvarioSetRotation(DisplayOrientation orientation)
{
  Display::Rotate(orientation);

  File::WriteExisting(Path("/sys/class/graphics/fbcon/rotate"),
                      fmt::format_int{ToConfigRotation(orientation)}.c_str());

  OpenvarioSaveRotation(orientation);
}

SSHStatus
OpenvarioGetSSHStatus()
{
  auto connection = ODBus::Connection::GetSystem();

  if (Systemd::IsUnitEnabled(connection, "dropbear.socket")) {
    return SSHStatus::ENABLED;
  } else if (Systemd::IsUnitActive(connection, "dropbear.socket")) {
    return SSHStatus::TEMPORARY;
  } else {
    return SSHStatus::DISABLED;
  }
}

void
OpenvarioEnableSSH(bool temporary)
{
  auto connection = ODBus::Connection::GetSystem();
  const ODBus::ScopeMatch job_removed_match{connection, Systemd::job_removed_match};

  if (temporary)
    Systemd::DisableUnitFile(connection, "dropbear.socket");
  else
    Systemd::EnableUnitFile(connection, "dropbear.socket");

  Systemd::StartUnit(connection, "dropbear.socket");
}

void
OpenvarioDisableSSH()
{
  auto connection = ODBus::Connection::GetSystem();
  const ODBus::ScopeMatch job_removed_match{connection, Systemd::job_removed_match};

  Systemd::DisableUnitFile(connection, "dropbear.socket");
  Systemd::StopUnit(connection, "dropbear.socket");
}

std::string
OpenvarioGetMainApp()
{
  std::map<std::string, std::string, std::less<>> map;
  LoadConfigFile(map, Path(boot_config));

  const auto i = map.find("main_app");
  return i != map.end() ? i->second : std::string{};
}

void
OpenvarioSetMainApp(std::string_view name)
{
  std::map<std::string, std::string, std::less<>> map;
  LoadConfigFile(map, Path(boot_config));
  map.insert_or_assign("main_app", std::string{name});
  WriteConfigFile(map, Path(boot_config));
}

std::string
OpenvarioGetImageName()
{
  char line[256];
  if (!File::ReadString(Path("/boot/image-version-info"),
                        line, sizeof(line)))
    return {};

  std::string_view first_line{line};
  first_line = first_line.substr(0, first_line.find_first_of("\r\n"));
  return std::string{OpenvarioImageName(first_line)};
}
