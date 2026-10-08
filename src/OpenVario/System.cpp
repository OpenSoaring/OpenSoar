// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "System.hpp"
#include "system/FileUtil.hpp"
#include "system/Path.hpp"
#include "io/KeyValueFileReader.hpp"
#include "io/FileOutputStream.hxx"
#include "io/BufferedOutputStream.hxx"
#include "io/FileLineReader.hpp"
#include "DisplayOrientation.hpp"

#include <fmt/format.h>

#include <map>

#include <unistd.h>

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

bool
OpenvarioIsUsbStickMounted() noexcept
try {
  /* Without a stick there is nothing to mount, and looking into the
     directory would only make systemd wait for the device until its
     timeout, while this thread hangs; the journal then shows "Timed
     out waiting for device USB stick".  usb-usbstick.mount of the
     image mounts this device. */
  if (access("/dev/sda1", F_OK) != 0)
    return false;

  /* looking into the directory makes systemd mount a stick that has
     been plugged in; the result does not matter */
  File::Exists(AllocatedPath::Build(Path(openvario_usb_stick), Path(".")));

  FileLineReaderA reader(Path("/proc/mounts"));
  const char *line;
  while ((line = reader.ReadLine()) != nullptr)
    if (HasRealMount(line, openvario_usb_stick))
      return true;

  return false;
} catch (...) {
  return false;
}
