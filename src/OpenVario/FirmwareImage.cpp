// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "FirmwareImage.hpp"
#include "Operation/Operation.hpp"
#include "Formatter/ByteSizeFormatter.hpp"
#include "Formatter/TimeFormatter.hpp"
#include "time/BrokenDateTime.hpp"
#include "io/FileReader.hxx"
#include "io/FileOutputStream.hxx"
#include "lib/fmt/SystemError.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "system/FileUtil.hpp"
#include "LogFile.hpp"
#include "Language/Language.hpp"
#include "util/StaticString.hxx"
#include "LocalPath.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

static constexpr const char *IMAGE_PATTERN = "*.img.gz";

/** where the OpenVario mounts the USB stick */
static constexpr const char *USB_IMAGE_DIRECTORIES[] = {
  "/usb/usbstick/openvario/images",
  "/usb/usbstick/openvario/download",
};

static AllocatedPath
GetDataPartitionPath(const char *name) noexcept
{
  const char *home = getenv("HOME");
  if (home == nullptr || *home == '\0')
    home = "/home/root";

  return AllocatedPath::Build(AllocatedPath::Build(Path{home}, Path{"data"}),
                              Path{name});
}

AllocatedPath
GetFirmwareDownloadPath() noexcept
{
  /* where the Download button puts images: on the device
     $HOME/data/download */
  if (auto path = GetProductDownloadsPath(); path != nullptr)
    return path;
  return GetDataPartitionPath("download");
}

AllocatedPath
GetFirmwareUpgradePath() noexcept
{
  return GetDataPartitionPath("images");
}

/**
 * "OV-3.2.20.1-CB2-CH57.img.gz" -> "OV-3.2.20.1-CB2-CH57"
 */
static std::string
ImageDisplayName(Path filename) noexcept
{
  std::string name = filename.c_str();
  for (const char *suffix : {".gz", ".img"}) {
    const std::size_t suffix_length = strlen(suffix);
    if (name.size() > suffix_length &&
        name.compare(name.size() - suffix_length, suffix_length, suffix) == 0)
      name.erase(name.size() - suffix_length);
  }
  return name;
}

std::string
ImageDeviceType(const char *image_name) noexcept
{
  std::string device;
  std::string_view rest = image_name;

  while (!rest.empty()) {
    const auto dash = rest.find('-');
    const auto part = rest.substr(0, dash);
    rest = dash == rest.npos ? std::string_view{} : rest.substr(dash + 1);

    if (part.empty() || (part[0] >= '0' && part[0] <= '9') ||
        part == "OV" || part == "testing")
      /* the product prefix, the version (one or more numeric parts)
         and the testing marker are not the hardware */
      continue;

    if (!device.empty())
      device += '-';
    device += part;
  }

  return device;
}

/**
 * The second row of the picker: path, size and date.
 */
static std::string
DescribeImage(Path path) noexcept
{
  std::string text = path.c_str();

  if (File::Exists(path)) {
    char size[32];
    FormatByteSize(size, sizeof(size), File::GetSize(path));
    char stamp[32];
    FormatISO8601(stamp, BrokenDateTime{File::GetLastModification(path)});
    text += ", ";
    text += size;
    text += ", ";
    text += stamp;
  }

  return text;
}

class ImageCollector final : public File::Visitor {
  std::vector<FirmwareImage> &images;
  const bool deletable;

public:
  ImageCollector(std::vector<FirmwareImage> &_images, bool _deletable) noexcept
    :images(_images), deletable(_deletable) {}

  void Visit(Path path, Path filename) override {
    /* the same directory may be reached under two names (a symlink);
       one entry is enough */
    for (const auto &i : images)
      if (i.path == path)
        return;

    images.push_back({ImageDisplayName(filename), AllocatedPath(path),
                      DescribeImage(path), deletable});
  }
};

static void
CollectImages(std::vector<FirmwareImage> &images, Path directory,
              bool deletable) noexcept
{
  if (directory == nullptr || !Directory::Exists(directory))
    return;

  const std::size_t before = images.size();
  ImageCollector collector(images, deletable);
  Directory::VisitSpecificFiles(directory, IMAGE_PATTERN, collector, false);
  LogFormat("firmware images: %u in %s", unsigned(images.size() - before),
            directory.c_str());
}

std::vector<FirmwareImage>
FindFirmwareImages() noexcept
{
  std::vector<FirmwareImage> images;

  CollectImages(images, GetFirmwareDownloadPath(), true);

  for (const char *usb : USB_IMAGE_DIRECTORIES)
    CollectImages(images, Path{usb}, false);

  std::sort(images.begin(), images.end(),
            [](const FirmwareImage &a, const FirmwareImage &b) {
              return a.name < b.name;
            });
  return images;
}

static void
MakeDirectory(Path path)
{
  Directory::CreateRecursive(path);
  if (!Directory::Exists(path))
    throw FmtRuntimeError("Failed to create {}", path.c_str());
}

/**
 * Copy the file in steps of 1 MiB, reporting the progress in MiB.
 * The copy gets a temporary name until it is complete, so a cancelled
 * or failed copy never looks like an image.
 *
 * @return false if the user cancelled
 */
static bool
CopyWithProgress(Path src, Path dest, OperationEnvironment &env)
{
  FileReader reader{src};
  const auto size = reader.GetSize();

  struct statvfs fs;
  if (const auto parent = dest.GetParent();
      statvfs(parent.c_str(), &fs) == 0 &&
      uint_least64_t(fs.f_bavail) * fs.f_frsize < size)
    throw FmtRuntimeError("Not enough space for {} in {}",
                          src.GetBase().c_str(), parent.c_str());

  static constexpr std::size_t CHUNK = 1024 * 1024;
  env.SetProgressRange(unsigned(size / CHUNK) + 1);

  /* FileOutputStream writes to a hidden temporary file and renames it
     on Commit(); without Commit() the temporary file is removed */
  FileOutputStream out{dest};

  auto buffer = std::make_unique<std::byte[]>(CHUNK);
  uint_least64_t done = 0;
  while (true) {
    if (env.IsCancelled())
      return false;

    const std::size_t n = reader.Read({buffer.get(), CHUNK});
    if (n == 0)
      break;

    out.Write({buffer.get(), n});
    done += n;
    env.SetProgressPosition(unsigned(done / CHUNK));
  }

  out.Commit();
  return true;
}

/**
 * Empty the upgrade directory.  Hard links are simply removed; a file
 * that is the only name of its data is an image put there by an older
 * version and is moved into the download directory.
 */
static void
ClearUpgradeDirectory(Path upgrade, Path download)
{
  DIR *dir = opendir(upgrade.c_str());
  if (dir == nullptr)
    return;

  std::vector<std::string> names;
  while (const auto *e = readdir(dir))
    if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0)
      names.emplace_back(e->d_name);
  closedir(dir);

  for (const auto &name : names) {
    const auto path = AllocatedPath::Build(upgrade, Path{name.c_str()});

    struct stat st;
    if (lstat(path.c_str(), &st) < 0)
      continue;

    if (S_ISREG(st.st_mode) && st.st_nlink == 1) {
      auto target = AllocatedPath::Build(download, Path{name.c_str()});
      for (unsigned i = 1; File::ExistsAny(target); ++i)
        target = AllocatedPath::Build(download,
                                      Path{(name + "." + std::to_string(i)).c_str()});
      if (rename(path.c_str(), target.c_str()) < 0)
        throw FmtErrno("Failed to move {} to {}", path.c_str(), target.c_str());
      LogFormat("firmware images: moved %s to %s", path.c_str(), target.c_str());
      continue;
    }

    if (S_ISDIR(st.st_mode))
      continue;

    if (unlink(path.c_str()) < 0)
      throw FmtErrno("Failed to remove {}", path.c_str());
  }
}

AllocatedPath
StageFirmwareImage(Path image, OperationEnvironment &env)
{
  const auto download = GetFirmwareDownloadPath();
  const auto upgrade = GetFirmwareUpgradePath();
  MakeDirectory(download);
  MakeDirectory(upgrade);

  const Path name = image.GetBase();

  /* the source of the hard link must live on the data partition: an
     image from the stick is copied into the download directory first */
  AllocatedPath source{image};
  if (image.GetParent() != download) {
    source = AllocatedPath::Build(download, name);
    if (source == image) {
      /* already there under another spelling of the path */
    } else if (File::Exists(source) &&
               File::GetSize(source) == File::GetSize(image)) {
      LogFormat("firmware images: %s is already in %s", name.c_str(),
                download.c_str());
    } else {
      StaticString<256> text;
      text.Format(_("Copying to the download directory: %s"), name.c_str());
      env.SetText(text);
      if (!CopyWithProgress(image, source, env))
        return nullptr;
      LogFormat("firmware images: copied %s to %s", image.c_str(),
                source.c_str());
    }
  }

  ClearUpgradeDirectory(upgrade, download);

  auto entry = AllocatedPath::Build(upgrade, name);
  if (link(source.c_str(), entry.c_str()) < 0) {
    if (errno != EXDEV)
      throw FmtErrno("Failed to link {} to {}", entry.c_str(), source.c_str());

    /* the download directory is on another file system (not on the
       device itself, where both are on the data partition): a copy
       has to do */
    StaticString<256> text;
    text.Format(_("Copying: %s"), name.c_str());
    env.SetText(text);
    if (!CopyWithProgress(source, entry, env))
      return nullptr;
  }

  sync();
  LogFormat("firmware images: %s links to %s", entry.c_str(), source.c_str());
  return entry;
}
