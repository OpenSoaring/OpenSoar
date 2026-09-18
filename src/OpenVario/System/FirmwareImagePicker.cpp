// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "FirmwareImagePicker.hpp"
#include "OpenVarioDevice.hpp"
#ifndef OPENVARIOBASEMENU
#include "BackendComponents.hpp"
#include "Components.hpp"
#include "Storage/StorageDevice.hpp"
#include "Storage/StorageManager.hpp"
#endif
#include "Dialogs/ListPicker.hpp"
#include "Form/DataField/File.hpp"
#include "Language/Language.hpp"
#include "LocalPath.hpp"
#include "LogFile.hpp"
#include "Look/DialogLook.hpp"
#include "Renderer/TwoTextRowsRenderer.hpp"
#include "Formatter/ByteSizeFormatter.hpp"
#include "Formatter/TimeFormatter.hpp"
#include "time/BrokenDateTime.hpp"
#include "UIGlobals.hpp"
#include "net/http/Features.hpp"
#include "system/FileUtil.hpp"
#include "ui/canvas/Canvas.hpp"
#include "util/StringCompare.hxx"

#include <string.h>

#ifdef HAVE_DOWNLOAD_MANAGER
#include "Dialogs/DownloadFilePicker.hpp"
#include "Repository/FileType.hpp"
#include "Repository/Glue.hpp"
#endif

#include <algorithm>

static constexpr const char *IMAGE_PATTERN = "*.img.gz";

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

/**
 * The second row: the path with forward slashes throughout (one
 * convention for every platform, and no doubled separator where a
 * drive root met the directory), then size and date.
 */
static std::string
DescribeImage(Path path) noexcept
{
  std::string text = path.c_str();
  for (auto &ch : text)
    if (ch == '\\')
      ch = '/';

  for (std::size_t i; (i = text.find("//")) != std::string::npos;)
    text.erase(i, 1);

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

public:
  explicit ImageCollector(std::vector<FirmwareImage> &_images) noexcept
    :images(_images) {}

  void Visit(Path path, Path filename) override {
    /* the same directory may be reached under two names (a symlink,
       or the data directory listed twice); one entry is enough */
    for (const auto &i : images)
      if (i.path == path)
        return;

    images.push_back({ImageDisplayName(filename), AllocatedPath(path),
                      DescribeImage(path)});
  }
};

static void
CollectImages(std::vector<FirmwareImage> &images, Path directory) noexcept
{
  if (directory == nullptr || directory.empty())
    return;

  if (!Directory::Exists(directory)) {
    LogFormat("firmware images: no directory %s", directory.c_str());
    return;
  }

  const std::size_t before = images.size();
  ImageCollector collector(images);
  Directory::VisitSpecificFiles(directory, IMAGE_PATTERN, collector, false);
  LogFormat("firmware images: %u in %s", unsigned(images.size() - before),
            directory.c_str());
}

std::vector<FirmwareImage>
FindFirmwareImages() noexcept
{
  std::vector<FirmwareImage> images;

  /* the download directory: on the device /home/root/data/download
     itself, on a PC or a phone the OpenVario subdirectory of the
     user's Downloads folder - where the Download button puts images
     as well.  Only the directory itself, never its subdirectories:
     old images can be moved aside into one */
  CollectImages(images, GetProductDownloadsPath());

#ifndef IS_OPENVARIO_CB2
  /* a development machine with OPENVARIO_ROOT sees the device's
     download directory below that root */
  if (ovdevice.HasSystemRoot())
    CollectImages(images, ovdevice.MapSystemPath(Path("/home/root/data/download")));
#endif

  /* the USB stick as the OpenVario mounts it (under OPENVARIO_ROOT on
     a development machine) */
  CollectImages(images, ovdevice.MapSystemPath(Path("/usb/usbstick/openvario/images")));

#ifndef OPENVARIOBASEMENU
  /* removable drives the storage layer knows about - on a PC this is
     the stick in a USB port, carrying openvario/images like the real
     one (the base menu has no storage manager and no need for one: on
     the device the stick is the fixed mount above) */
  if (backend_components != nullptr &&
      backend_components->storage_manager != nullptr) {
    for (const auto &device : backend_components->storage_manager->GetDevices()) {
      if (device->GetKind() != StorageDevice::Kind::Removable)
        continue;

      /* Name() is the mount point for file system devices */
      const std::string root = device->Name();
      if (root.empty())
        continue;

      CollectImages(images, AllocatedPath::Build(Path(root.c_str()),
                                                 Path("openvario/images")));
    }
  }
#endif

  std::sort(images.begin(), images.end(),
            [](const FirmwareImage &a, const FirmwareImage &b) {
              return a.name < b.name;
            });
  return images;
}

class ImageRowRenderer final : public ListItemRenderer {
  const std::vector<FirmwareImage> &images;
  TwoTextRowsRenderer row_renderer;

public:
  explicit ImageRowRenderer(const std::vector<FirmwareImage> &_images) noexcept
    :images(_images) {}

  unsigned CalculateLayout(const DialogLook &look) noexcept {
    return row_renderer.CalculateLayout(*look.list.font, look.small_font);
  }

  void OnPaintItem(Canvas &canvas, const PixelRect rc,
                   unsigned i) noexcept override {
    row_renderer.DrawFirstRow(canvas, rc, images[i].name.c_str());
    row_renderer.DrawSecondRow(canvas, rc, images[i].detail.c_str());
  }
};

bool
PickFirmwareImage(const char *caption, DataField &_df,
                  const char *help_text) noexcept
{
  auto &df = (FileDataField &)_df;

  const char *extra_caption = nullptr;
#ifdef HAVE_DOWNLOAD_MANAGER
  if (FileTypeSupportsDownload(FileType::IMAGE))
    extra_caption = _("Download");
#endif

  while (true) {
    const auto images = FindFirmwareImages();

    /* preselect the image the row shows, if it is in the list */
    unsigned initial = 0;
    if (const Path current = df.GetValue(); current != nullptr)
      for (unsigned i = 0; i < images.size(); ++i)
        if (images[i].path == current ||
            StringIsEqual(images[i].name.c_str(), current.c_str())) {
          initial = i;
          break;
        }

    ImageRowRenderer renderer(images);
    const int result = ListPicker(caption, images.size(), initial,
                                  renderer.CalculateLayout(UIGlobals::GetDialogLook()),
                                  renderer, false, help_text, nullptr,
                                  extra_caption);

#ifdef HAVE_DOWNLOAD_MANAGER
    if (result == mrExtra) {
      /* a download lands in the data directory, which the list covers:
         show the list again, now with the new file */
      const auto downloaded = DownloadFilePicker(FileType::IMAGE);
      if (downloaded == nullptr)
        continue;

      df.ForceModify(downloaded);
      return true;
    }
#endif

    if (result < 0)
      return false;

    df.ForceModify(images[result].path);
    return true;
  }
}
