// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Parser.hpp"
#include "FileRepository.hpp"
#include "io/LineReader.hpp"
#include "util/StringStrip.hxx"
#include "util/HexString.hpp"
#include "system/Path.hpp"
#include "util/StringAPI.hxx"

#include <string>

/**
 * Parses a line of the repository file.
 * Each line is of the form `name = value`
 * @returns A pointer to the value field.
 */
static const char *
ParseLine(char *line)
{
  char *separator = strchr(line, '=');
  if (separator == nullptr)
    /* malformed line */
    return nullptr;

  char *p = StripRight(line, separator);
  if (p == line)
    /* empty name */
    return nullptr;

  *p = 0;

  char *value = const_cast<char *>(StripLeft(separator + 1));
  StripRight(value);
  return value;
}

/**
 * The platform name the program repositories use in the "device" field
 * of their packages (see make_repository.py on the web site), or
 * nullptr where no package exists: on the OpenVario the program comes
 * with the firmware image.
 */
static constexpr const char *
OwnPackagePlatform() noexcept
{
#if defined(ANDROID) && defined(__aarch64__)
  return "android-arm64";
#elif defined(ANDROID)
  return "android";
#elif defined(KOBO)
  return "kobo";
#elif defined(_WIN64)
  return "win64";
#elif defined(_WIN32)
  return "win32";
#elif defined(IS_OPENVARIO) || defined(__APPLE__)
  return nullptr;
#elif defined(__linux__) && defined(__x86_64__)
  return "linux-amd64";
#elif defined(__linux__) && defined(__aarch64__)
  return "linux-arm64";
#else
  return nullptr;
#endif
}

/**
 * Is a program package with this "device" field meant for this
 * platform?  The package with all Android ABIs ("android") also runs
 * on a 64 bit ARM device, so that one sees both APKs.
 */
static bool
IsOwnPackage(const std::string &device) noexcept
{
  const char *own = OwnPackagePlatform();
  if (own == nullptr)
    return false;

  if (device == own)
    return true;

#ifdef ANDROID
  return device == "android";
#else
  return false;
#endif
}

[[gnu::pure]]
static bool
IsProgramPackageType(const char *value) noexcept
{
  return StringIsEqual(value, "win64-zip") ||
    StringIsEqual(value, "win32-zip") ||
    StringIsEqual(value, "deb") ||
    StringIsEqual(value, "kobo") ||
    StringIsEqual(value, "apk");
}

static bool
Commit(FileRepository &repository, AvailableFile &file,
       bool &program_package, std::string &device)
{
  /* a program package only counts on the platform it was built for;
     on every other one the entry is dropped, so the pilot is not
     offered a download that cannot run here */
  if (program_package) {
    program_package = false;
    if (IsOwnPackage(device))
      file.type = FileType::PROGRAM;
    else
      file.Clear();
  }
  device.clear();

  if (file.IsEmpty())
    return true;

  const Path path(file.name.c_str());
  if (!path.IsValidFilename())
    return false;

  if (!file.IsValid())
    return false;

  repository.files.emplace_back(std::move(file));
  file.Clear();
  return true;
}

bool
ParseFileRepository(FileRepository &repository, NLineReader &reader)
{
  AvailableFile file;
  file.Clear();

  /* the type and platform of a program package; whether the entry is
     kept is decided when it is complete, because "device" may come
     before or after "type" */
  bool program_package = false;
  std::string device;

  char *line;
  while ((line = reader.ReadLine()) != nullptr) {
    line = const_cast<char *>(StripLeft(line));
    if (*line == 0 || *line == '#')
      continue;

    const char *name = line, *value = ParseLine(line);
    if (value == nullptr)
      return false;

    if (StringIsEqual(name, "name")) {
      if (!Commit(repository, file, program_package, device))
        return false;

      file.name.assign(value);
    } else if (file.IsEmpty()) {
      /* ignore */
    } else if (StringIsEqual(name, "uri")) {
      file.uri.assign(value);
    } else if (StringIsEqual(name, "description")) {
      file.description.assign(value);
    } else if (StringIsEqual(name, "device")) {
      device.assign(value);
    } else if (StringIsEqual(name, "area")) {
      file.area = value;
    } else if (StringIsEqual(name, "type")) {
      if (StringIsEqual(value, "airspace"))
        file.type = FileType::AIRSPACE;
      else if (StringIsEqual(value, "waypoint-details"))
        file.type = FileType::WAYPOINTDETAILS;
      else if (StringIsEqual(value, "waypoint"))
        file.type = FileType::WAYPOINT;
      else if (StringIsEqual(value, "map"))
        file.type = FileType::MAP;
      else if (StringIsEqual(value, "flarmnet"))
        file.type = FileType::FLARMNET;
      else if (StringIsEqual(value, "rasp"))
        file.type = FileType::RASP;
      else if (StringIsEqual(value, "xci"))
        file.type = FileType::XCI;
      else if (StringIsEqual(value, "task"))
        file.type = FileType::TASK;
      else if (StringIsEqual(value, "checklist"))
        file.type = FileType::CHECKLIST;
      else if (StringIsEqual(value, "frequencies"))
        file.type = FileType::FREQUENCIES;
      else if (IsProgramPackageType(value))
        program_package = true;
#ifdef IS_OPENVARIO
      else if (StringIsEqual(value, "ov-image"))
        file.type = FileType::OV_IMAGE;
      else if (StringIsEqual(value, "ov-upgrade"))
        file.type = FileType::OV_UPGRADE;
      else if (StringIsEqual(value, "ov-ipk"))
        file.type = FileType::OV_IPK;
#endif  // IS_OPENVARIO
    } else if (StringIsEqual(name, "update")) {
      unsigned year, month, day;
      if (sscanf(value, "%04u-%02u-%02u", &year, &month, &day) == 3)
        file.update_date = BrokenDate(year, month, day);
    } else if (StringIsEqual(name, "sha256")) {
      try {
        file.sha256_hash = ParseHexString<32>(std::string_view(value));
      } catch (std::exception &e) {
        // Parsing failed, sha256_hash stays zeroed
      }
    }
  }

  return Commit(repository, file, program_package, device);
}
