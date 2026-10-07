// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "FirmwareImagePicker.hpp"
#include "FirmwareImage.hpp"
#include "System.hpp"
#include "Dialogs/ListPicker.hpp"
#include "Dialogs/Message.hpp"
#include "Form/Form.hpp"
#include "Language/Language.hpp"
#include "LogFile.hpp"
#include "Look/DialogLook.hpp"
#include "Renderer/TwoTextRowsRenderer.hpp"
#include "UIGlobals.hpp"
#include "net/http/Features.hpp"
#include "system/FileUtil.hpp"
#include "util/StaticString.hxx"
#include "util/StringAPI.hxx"
#include "util/StringPart.hxx"

#if defined(HAVE_DOWNLOAD_MANAGER) && defined(IS_OPENVARIO)
/* the repository knows firmware images on the OpenVario targets only */
#define HAVE_IMAGE_DOWNLOAD
#include "Dialogs/DownloadFilePicker.hpp"
#include "Repository/FileType.hpp"
#include "Repository/Glue.hpp"
#endif

#include <algorithm>

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

/**
 * The Delete button: remove the image from the download directory
 * after a confirmation.  Images on a stick are left alone - the stick
 * is the user's archive, and the button is for downloads that are no
 * longer needed.
 */
static void
DeleteImage(const FirmwareImage &image) noexcept
{
  StaticString<512> message;

  if (!image.deletable) {
    message.Format("%s\n%s\n\n%s", image.name.c_str(), image.path.c_str(),
                   _("This image is not in the download directory; only downloaded images can be deleted here."));
    ShowMessageBox(message, _("Delete"), MB_OK | MB_ICONINFORMATION);
    return;
  }

  message.Format("%s\n%s\n\n%s", image.name.c_str(), image.path.c_str(),
                 _("Delete this image?"));
  if (ShowMessageBox(message, _("Delete"), MB_YESNO | MB_ICONQUESTION) != IDYES)
    return;

  if (!File::Delete(image.path)) {
    message.Format("%s\n%s", _("Failed to delete the image."),
                   image.path.c_str());
    ShowMessageBox(message, _("Delete"), MB_OK | MB_ICONERROR);
    return;
  }

  LogFormat("firmware images: deleted %s", image.path.c_str());
}

AllocatedPath
PickFirmwareImage(const char *caption, Path current) noexcept
{
  const char *download_caption = nullptr;
#ifdef HAVE_IMAGE_DOWNLOAD
  if (FileTypeSupportsDownload(FileType::OV_IMAGE))
    download_caption = _("Download");
#endif

  /* the hardware this device runs on, from the running image's name;
     empty on a PC, and then there is nothing to filter by */
  const std::string device = ImageDeviceType(OpenvarioGetImageName().c_str());

  /* the filter state survives between two openings of the picker in
     one session, and the download list shares it */
  static bool device_only = true;

  while (true) {
    auto images = FindFirmwareImages();

    if (!device.empty() && device_only)
      std::erase_if(images, [&device](const FirmwareImage &image) {
        return !StringHasPart(image.name, device);
      });

    StaticString<32> filter_caption;
    if (device.empty())
      filter_caption.clear();
    else if (device_only)
      filter_caption = _("Show all");
    else
      filter_caption.Format(_("%s only"), device.c_str());

    unsigned initial = 0;
    if (current != nullptr)
      for (unsigned i = 0; i < images.size(); ++i)
        if (images[i].path == current ||
            StringIsEqual(images[i].name.c_str(), current.c_str())) {
          initial = i;
          break;
        }

    ImageRowRenderer renderer(images);
    unsigned cursor = 0;
    const int result =
      ListPicker(caption, images.size(), initial,
                 renderer.CalculateLayout(UIGlobals::GetDialogLook()),
                 renderer, false,
                 _("Choose the image to upgrade to.  Images are looked for "
                   "in the download directory and on the USB stick "
                   "(openvario/images)."),
                 nullptr, download_caption,
                 images.empty() ? nullptr : _("Delete"),
                 &cursor,
                 filter_caption.empty() ? nullptr : filter_caption.c_str());

    if (result == mrExtra3) {
      device_only = !device_only;
      continue;
    }

    if (result == mrExtra2) {
      /* the user may want to clean out several old downloads in one
         go, so the list comes back afterwards */
      DeleteImage(images[cursor]);
      continue;
    }

#ifdef HAVE_IMAGE_DOWNLOAD
    if (result == mrExtra) {
      DownloadNameFilter filter{device.c_str(), device.c_str(), device_only};
      auto downloaded =
        DownloadFilePicker(FileType::OV_IMAGE,
                           device.empty() ? nullptr : &filter);
      if (!device.empty())
        device_only = filter.enabled;
      if (downloaded == nullptr)
        continue;

      return downloaded;
    }
#endif

    if (result < 0)
      return nullptr;

    return std::move(images[result].path);
  }
}
