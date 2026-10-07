// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "FirmwareImagePicker.hpp"
#include "FirmwareImage.hpp"
#include "System.hpp"
#include "Dialogs/ListPicker.hpp"
#include "Form/Form.hpp"
#include "Language/Language.hpp"
#include "Look/DialogLook.hpp"
#include "Renderer/TwoTextRowsRenderer.hpp"
#include "UIGlobals.hpp"
#include "util/StaticString.hxx"

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

AllocatedPath
PickFirmwareImage(const char *caption) noexcept
{
  /* the hardware this device runs on, from the running image's name;
     empty on a PC, and then there is nothing to filter by */
  const std::string device = ImageDeviceType(OpenvarioGetImageName().c_str());

  /* the filter state survives between two openings of the picker */
  static bool device_only = true;

  while (true) {
    auto images = FindFirmwareImages();

    if (!device.empty() && device_only)
      std::erase_if(images, [&device](const FirmwareImage &image) {
        return image.name.find(device) == std::string::npos;
      });

    StaticString<32> filter_caption;
    if (device.empty())
      filter_caption.clear();
    else if (device_only)
      filter_caption = _("Show all");
    else
      filter_caption.Format(_("%s only"), device.c_str());

    ImageRowRenderer renderer(images);
    const int result =
      ListPicker(caption, images.size(), 0,
                 renderer.CalculateLayout(UIGlobals::GetDialogLook()),
                 renderer, false,
                 _("Choose the image to upgrade to.  Images are looked for "
                   "in data/download and on the USB stick "
                   "(openvario/images)."),
                 nullptr,
                 filter_caption.empty() ? nullptr : filter_caption.c_str());

    if (result == mrExtra) {
      device_only = !device_only;
      continue;
    }

    if (result < 0)
      return nullptr;

    return std::move(images[result].path);
  }
}
